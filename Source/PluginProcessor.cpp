#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <complex>

namespace polezero
{
    PoleZeroProcessor::PoleZeroProcessor()
        : juce::AudioProcessor (BusesProperties()
                                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "state", createLayout())
    {
    }

    juce::AudioProcessorValueTreeState::ParameterLayout PoleZeroProcessor::createLayout()
    {
        using P = juce::AudioParameterFloat;
        using B = juce::AudioParameterBool;
        using C = juce::AudioParameterChoice;

        const float pi = juce::MathConstants<float>::pi;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto addRadius = [&] (const char* id, const char* name, float def)
        {
            layout.add (std::make_unique<P> (juce::ParameterID { id, 1 }, name,
                juce::NormalisableRange<float> (0.0f, kRadiusMax, 0.0f, 1.0f), def));
        };

        auto addAngle = [&] (const char* id, const char* name, float def)
        {
            layout.add (std::make_unique<P> (juce::ParameterID { id, 1 }, name,
                juce::NormalisableRange<float> (-pi, pi, 0.0f, 1.0f), def));
        };

        // Defaults place p2 at conj(p1) and z2 at conj(z1) so unlocking from
        // the factory state still draws a symmetric pair.
        addRadius (kPole1Radius, "Pole 1 Radius", 0.85f);
        addAngle  (kPole1Angle,  "Pole 1 Angle",  pi * 0.25f);
        addRadius (kPole2Radius, "Pole 2 Radius", 0.85f);
        addAngle  (kPole2Angle,  "Pole 2 Angle", -pi * 0.25f);

        addRadius (kZero1Radius, "Zero 1 Radius", 1.0f);
        addAngle  (kZero1Angle,  "Zero 1 Angle",  pi);
        addRadius (kZero2Radius, "Zero 2 Radius", 1.0f);
        addAngle  (kZero2Angle,  "Zero 2 Angle",  pi);

        layout.add (std::make_unique<B> (juce::ParameterID { kLockConjugate, 1 },
                                         "Lock Conjugate", true));

        juce::StringArray boundaryChoices;
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryChoices.add (boundaryConditionName (static_cast<BoundaryCondition> (i)));

        layout.add (std::make_unique<C> (juce::ParameterID { kBoundary, 1 },
                                         "Boundary",
                                         boundaryChoices,
                                         static_cast<int> (BoundaryCondition::Tanh)));

        layout.add (std::make_unique<P> (juce::ParameterID { kBoundaryLevel, 1 },
                                         "Boundary Level",
                                         juce::NormalisableRange<float> (0.05f, 4.0f, 0.0f, 0.4f),
                                         1.0f));

        layout.add (std::make_unique<P> (juce::ParameterID { kGainDb, 1 },
                                         "Gain",
                                         juce::NormalisableRange<float> (-24.0f, 24.0f, 0.0f, 1.0f),
                                         0.0f));

        return layout;
    }

    void PoleZeroProcessor::prepareToPlay (double, int samplesPerBlock)
    {
        filter.prepare();
        filter.reset();

        const size_t channels = static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels()));
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            channels,
            static_cast<size_t> (kOversampleStages),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));
        oversampler->reset();
        setLatencySamples (static_cast<int> (oversampler->getLatencyInSamples()));
    }

    bool PoleZeroProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;
        return out == layouts.getMainInputChannelSet();
    }

    void PoleZeroProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        using C = std::complex<float>;

        const float r1p     = apvts.getRawParameterValue (kPole1Radius)->load();
        const float t1p     = apvts.getRawParameterValue (kPole1Angle)->load();
        const float r2p     = apvts.getRawParameterValue (kPole2Radius)->load();
        const float t2p     = apvts.getRawParameterValue (kPole2Angle)->load();
        const float r1z     = apvts.getRawParameterValue (kZero1Radius)->load();
        const float t1z     = apvts.getRawParameterValue (kZero1Angle)->load();
        const float r2z     = apvts.getRawParameterValue (kZero2Radius)->load();
        const float t2z     = apvts.getRawParameterValue (kZero2Angle)->load();
        const bool  locked  = apvts.getRawParameterValue (kLockConjugate)->load() > 0.5f;
        const int   bcIndex = static_cast<int> (apvts.getRawParameterValue (kBoundary)->load());
        const float bLevel  = apvts.getRawParameterValue (kBoundaryLevel)->load();
        const float gainDb  = apvts.getRawParameterValue (kGainDb)->load();

        const auto bc = static_cast<BoundaryCondition> (juce::jlimit (0,
            static_cast<int> (BoundaryCondition::NumBoundaryConditions) - 1, bcIndex));

        // Pole/zero angles live in user-Nyquist space; the filter runs at
        // kOversampleFactor x that rate, so scale angles down to keep the
        // perceived pitch constant.
        const float angleScale = 1.0f / static_cast<float> (kOversampleFactor);
        const C p1 = std::polar (r1p, t1p * angleScale);
        const C p2 = locked ? std::conj (p1) : std::polar (r2p, t2p * angleScale);
        const C z1 = std::polar (r1z, t1z * angleScale);
        const C z2 = locked ? std::conj (z1) : std::polar (r2z, t2z * angleScale);

        // Auto-normalise against the linear filter's magnitude at omega = pi/2
        // so the user's gain control stays meaningful as the poles move. The
        // in-DSP boundary condition shapes the runaway on top of that.
        auto magnitudeAt = [&] (float omega)
        {
            const C ePos = std::polar (1.0f, -omega); // z^-1 = e^{-j omega}
            const C num = (1.0f - z1 * ePos) * (1.0f - z2 * ePos);
            const C den = (1.0f - p1 * ePos) * (1.0f - p2 * ePos);
            return std::abs (num / den);
        };

        const float normRef = juce::jmax (1.0e-6f, magnitudeAt (juce::MathConstants<float>::halfPi));
        const float gainLin = juce::Decibels::decibelsToGain (gainDb) / normRef;

        filter.setCoefficients (p1, p2, z1, z2, gainLin);
        filter.setBoundary (bc, bLevel);

        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = oversampler->processSamplesUp (block);

        const int upNumSamples  = static_cast<int> (upBlock.getNumSamples());
        const int upNumChannels = static_cast<int> (upBlock.getNumChannels());

        if (upNumChannels >= 2)
        {
            auto* L = upBlock.getChannelPointer (0);
            auto* R = upBlock.getChannelPointer (1);
            for (int n = 0; n < upNumSamples; ++n)
            {
                const C y = filter.processSample (C { L[n], R[n] });
                L[n] = y.real();
                R[n] = y.imag();
            }
        }
        else if (upNumChannels == 1)
        {
            auto* M = upBlock.getChannelPointer (0);
            for (int n = 0; n < upNumSamples; ++n)
            {
                const C y = filter.processSample (C { M[n], 0.0f });
                M[n] = y.real();
            }
        }

        oversampler->processSamplesDown (block);

        juce::ignoreUnused (numChannels, numSamples);
    }

    juce::AudioProcessorEditor* PoleZeroProcessor::createEditor()
    {
        return new PoleZeroEditor (*this);
    }

    void PoleZeroProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void PoleZeroProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new polezero::PoleZeroProcessor();
}
