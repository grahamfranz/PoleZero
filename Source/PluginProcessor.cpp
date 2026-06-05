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
        using C = juce::AudioParameterChoice;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Radii can sit at or past the unit circle. The linear filter is
        // unstable there; the in-DSP boundary condition is what keeps the
        // output bounded and gives the runaway its character.
        layout.add (std::make_unique<P> (juce::ParameterID { kPoleRadius, 1 },
                                         "Pole Radius",
                                         juce::NormalisableRange<float> (0.0f, kRadiusMax, 0.0f, 1.0f),
                                         0.85f));

        layout.add (std::make_unique<P> (juce::ParameterID { kPoleAngle, 1 },
                                         "Pole Angle",
                                         juce::NormalisableRange<float> (0.0f, juce::MathConstants<float>::pi, 0.0f, 1.0f),
                                         juce::MathConstants<float>::pi * 0.25f));

        layout.add (std::make_unique<P> (juce::ParameterID { kZeroRadius, 1 },
                                         "Zero Radius",
                                         juce::NormalisableRange<float> (0.0f, kRadiusMax, 0.0f, 1.0f),
                                         1.0f));

        layout.add (std::make_unique<P> (juce::ParameterID { kZeroAngle, 1 },
                                         "Zero Angle",
                                         juce::NormalisableRange<float> (0.0f, juce::MathConstants<float>::pi, 0.0f, 1.0f),
                                         juce::MathConstants<float>::pi));

        juce::StringArray boundaryChoices;
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryChoices.add (boundaryConditionName (static_cast<BoundaryCondition> (i)));

        layout.add (std::make_unique<C> (juce::ParameterID { kBoundary, 1 },
                                         "Boundary",
                                         boundaryChoices,
                                         static_cast<int> (BoundaryCondition::Tanh)));

        // Boundary level: where the BC bites on the feedback sample.
        // Skewed so the middle of the knob lands near unity (0 dBFS-ish).
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

    void PoleZeroProcessor::prepareToPlay (double, int)
    {
        filter.prepare (getTotalNumOutputChannels());
        filter.reset();
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

        const float rP      = apvts.getRawParameterValue (kPoleRadius)->load();
        const float thetaP  = apvts.getRawParameterValue (kPoleAngle)->load();
        const float rZ      = apvts.getRawParameterValue (kZeroRadius)->load();
        const float thetaZ  = apvts.getRawParameterValue (kZeroAngle)->load();
        const int   bcIndex = static_cast<int> (apvts.getRawParameterValue (kBoundary)->load());
        const float bLevel  = apvts.getRawParameterValue (kBoundaryLevel)->load();
        const float gainDb  = apvts.getRawParameterValue (kGainDb)->load();

        const auto bc = static_cast<BoundaryCondition> (juce::jlimit (0,
            static_cast<int> (BoundaryCondition::NumBoundaryConditions) - 1, bcIndex));

        // Auto-normalise against the linear filter's magnitude at omega = pi/2
        // so the user's gain control stays meaningful when the pole moves.
        // This is purely linear-domain; the in-DSP boundary condition shapes
        // the runaway on top of it.
        auto magnitudeAt = [&] (float omega)
        {
            const std::complex<float> z = std::polar (1.0f, omega);
            const std::complex<float> num = 1.0f - 2.0f * rZ * std::cos (thetaZ) / z + (rZ * rZ) / (z * z);
            const std::complex<float> den = 1.0f - 2.0f * rP * std::cos (thetaP) / z + (rP * rP) / (z * z);
            return std::abs (num / den);
        };

        const float normRef = juce::jmax (1.0e-6f, magnitudeAt (juce::MathConstants<float>::halfPi));
        const float gainLin = juce::Decibels::decibelsToGain (gainDb) / normRef;

        filter.setCoefficients (rP, thetaP, rZ, thetaZ, gainLin);
        filter.setBoundary (bc, bLevel);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
                data[n] = filter.processSample (ch, data[n]);
        }
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
