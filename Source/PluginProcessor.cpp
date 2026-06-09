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

        // Piecewise centre-weighted mapping. Slider position 0.7 pins to
        // r = 1.0 (the unit circle, where the linear filter goes unstable).
        // Bottom 70% of slider covers the stable inside region 0..1.0 with a
        // sqrt curve, so resolution grows as r approaches the circle. Top 30%
        // covers the runaway region 1.0..1.6 with a square curve, so
        // resolution is highest just past the circle (where BC character
        // changes fastest) and lowest near 1.6 (where the BC has fully
        // saturated and small radius moves are inaudible).
        //
        // Assumes start = 0 and the unit circle sits at v = 1.0; both true
        // for kRadiusMax = 1.6. Continuous at the pivot.
        auto addRadius = [&] (const char* id, const char* name, float def)
        {
            juce::NormalisableRange<float> range (
                0.0f, kRadiusMax,
                [] (float, float end, float t) -> float
                {
                    constexpr float pivot = 0.7f;
                    if (t <= pivot)
                        return std::sqrt (t / pivot);
                    const float u = (t - pivot) / (1.0f - pivot);
                    return 1.0f + u * u * (end - 1.0f);
                },
                [] (float, float end, float v) -> float
                {
                    constexpr float pivot = 0.7f;
                    if (v <= 1.0f)
                        return v * v * pivot;
                    const float u = std::sqrt ((v - 1.0f) / (end - 1.0f));
                    return pivot + u * (1.0f - pivot);
                },
                [] (float, float, float v) { return v; });
            layout.add (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def));
        };

        auto addAngle = [&] (const char* id, const char* name, float def)
        {
            layout.add (std::make_unique<P> (juce::ParameterID { id, 1 }, name,
                juce::NormalisableRange<float> (-pi, pi, 0.0f, 1.0f), def));
        };

        // Defaults place p2 at conj(p1) and z2 at conj(z1) so unlocking from
        // the factory state still draws a symmetric pair.
        addRadius (kPole1Radius, "A Pole 1 Radius", 0.85f);
        addAngle  (kPole1Angle,  "A Pole 1 Angle",  pi * 0.25f);
        addRadius (kPole2Radius, "A Pole 2 Radius", 0.85f);
        addAngle  (kPole2Angle,  "A Pole 2 Angle", -pi * 0.25f);

        addRadius (kZero1Radius, "A Zero 1 Radius", 1.0f);
        addAngle  (kZero1Angle,  "A Zero 1 Angle",  pi);
        addRadius (kZero2Radius, "A Zero 2 Radius", 1.0f);
        addAngle  (kZero2Angle,  "A Zero 2 Angle",  pi);

        layout.add (std::make_unique<B> (juce::ParameterID { kLockConjugate, 1 },
                                         "A Lock Conjugate", true));

        // Stage B defaults: everything at the origin, which collapses the
        // biquad to identity (b0=1, all other coeffs=0). With routing default
        // = Series, the second stage is a pass-through and the plugin sounds
        // identical to the single-stage version until the user moves the
        // stage-B markers off the origin.
        addRadius (kPoleB1Radius, "B Pole 1 Radius", 0.0f);
        addAngle  (kPoleB1Angle,  "B Pole 1 Angle",  pi * 0.25f);
        addRadius (kPoleB2Radius, "B Pole 2 Radius", 0.0f);
        addAngle  (kPoleB2Angle,  "B Pole 2 Angle", -pi * 0.25f);

        addRadius (kZeroB1Radius, "B Zero 1 Radius", 0.0f);
        addAngle  (kZeroB1Angle,  "B Zero 1 Angle",  pi);
        addRadius (kZeroB2Radius, "B Zero 2 Radius", 0.0f);
        addAngle  (kZeroB2Angle,  "B Zero 2 Angle",  pi);

        layout.add (std::make_unique<B> (juce::ParameterID { kLockConjugateB, 1 },
                                         "B Lock Conjugate", true));

        juce::StringArray routingChoices;
        for (int i = 0; i < static_cast<int> (Routing::NumRoutings); ++i)
            routingChoices.add (routingName (static_cast<Routing> (i)));

        layout.add (std::make_unique<C> (juce::ParameterID { kRouting, 1 },
                                         "Routing",
                                         routingChoices,
                                         static_cast<int> (Routing::Series)));

        juce::StringArray boundaryChoices;
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryChoices.add (boundaryConditionName (static_cast<BoundaryCondition> (i)));

        layout.add (std::make_unique<C> (juce::ParameterID { kBoundary, 1 },
                                         "Boundary",
                                         boundaryChoices,
                                         static_cast<int> (BoundaryCondition::Tanh)));

        juce::StringArray tapChoices;
        for (int i = 0; i < static_cast<int> (BoundaryTap::NumBoundaryTaps); ++i)
            tapChoices.add (boundaryTapName (static_cast<BoundaryTap> (i)));

        layout.add (std::make_unique<C> (juce::ParameterID { kBoundaryTap, 1 },
                                         "BC Tap",
                                         tapChoices,
                                         static_cast<int> (BoundaryTap::Output)));

        // Boundary level is the BC threshold relative to the auto-normalised
        // filter peak. Defaults at ~+6dB headroom so the BC behaves as a
        // safety net catching runaway, not a permanent waveshaper.
        layout.add (std::make_unique<P> (juce::ParameterID { kBoundaryLevel, 1 },
                                         "Boundary Level",
                                         juce::NormalisableRange<float> (0.3f, 4.0f, 0.0f, 1.0f),
                                         2.0f));

        // Global one-sample feedback around the routing combine. At 0 the
        // plugin is the cascade/parallel of the two stages with no recursion
        // outside them; past ~0.95 the loop self-resonates and the BC inside
        // each stage shapes the otherwise-runaway tail. > 1.0 is permitted
        // because the BC is the safety net by design.
        layout.add (std::make_unique<P> (juce::ParameterID { kFeedback, 1 },
                                         "Feedback",
                                         juce::NormalisableRange<float> (0.0f, 1.2f, 0.0f, 1.0f),
                                         0.0f));

        layout.add (std::make_unique<P> (juce::ParameterID { kOutputDb, 1 },
                                         "Output",
                                         juce::NormalisableRange<float> (-24.0f, 24.0f, 0.0f, 1.0f),
                                         0.0f));

        return layout;
    }

    void PoleZeroProcessor::prepareToPlay (double, int samplesPerBlock)
    {
        filterA.prepare();
        filterA.reset();
        filterB.prepare();
        filterB.reset();
        probeA.reset();
        probeB.reset();
        probeCounter = 0;
        lastOutput = { 0.0f, 0.0f };

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

        // Stage A pole/zero/lock + tap.
        const float rA1p    = apvts.getRawParameterValue (kPole1Radius)->load();
        const float tA1p    = apvts.getRawParameterValue (kPole1Angle)->load();
        const float rA2p    = apvts.getRawParameterValue (kPole2Radius)->load();
        const float tA2p    = apvts.getRawParameterValue (kPole2Angle)->load();
        const float rA1z    = apvts.getRawParameterValue (kZero1Radius)->load();
        const float tA1z    = apvts.getRawParameterValue (kZero1Angle)->load();
        const float rA2z    = apvts.getRawParameterValue (kZero2Radius)->load();
        const float tA2z    = apvts.getRawParameterValue (kZero2Angle)->load();
        const bool  lockedA = apvts.getRawParameterValue (kLockConjugate)->load() > 0.5f;

        // Stage B pole/zero/lock.
        const float rB1p    = apvts.getRawParameterValue (kPoleB1Radius)->load();
        const float tB1p    = apvts.getRawParameterValue (kPoleB1Angle)->load();
        const float rB2p    = apvts.getRawParameterValue (kPoleB2Radius)->load();
        const float tB2p    = apvts.getRawParameterValue (kPoleB2Angle)->load();
        const float rB1z    = apvts.getRawParameterValue (kZeroB1Radius)->load();
        const float tB1z    = apvts.getRawParameterValue (kZeroB1Angle)->load();
        const float rB2z    = apvts.getRawParameterValue (kZeroB2Radius)->load();
        const float tB2z    = apvts.getRawParameterValue (kZeroB2Angle)->load();
        const bool  lockedB = apvts.getRawParameterValue (kLockConjugateB)->load() > 0.5f;

        // Shared BC, tap, routing, output trim.
        const int   bcIndex      = static_cast<int> (apvts.getRawParameterValue (kBoundary)->load());
        const int   tapIndex     = static_cast<int> (apvts.getRawParameterValue (kBoundaryTap)->load());
        const int   routingIndex = static_cast<int> (apvts.getRawParameterValue (kRouting)->load());
        const float bLevel       = apvts.getRawParameterValue (kBoundaryLevel)->load();
        const float feedback     = apvts.getRawParameterValue (kFeedback)->load();
        const float outputDb     = apvts.getRawParameterValue (kOutputDb)->load();

        const auto bc = static_cast<BoundaryCondition> (juce::jlimit (0,
            static_cast<int> (BoundaryCondition::NumBoundaryConditions) - 1, bcIndex));
        const int  clampedTapIndex = juce::jlimit (0,
            static_cast<int> (BoundaryTap::NumBoundaryTaps) - 1, tapIndex);
        const auto tap = static_cast<BoundaryTap> (clampedTapIndex);
        const auto routing = static_cast<Routing> (juce::jlimit (0,
            static_cast<int> (Routing::NumRoutings) - 1, routingIndex));

        // Switching tap mid-stream drops the recursion into a regime that
        // wasn't bounding the previous state — reset both stages so the new
        // tap starts clean across the chain. Also clear the feedback memory
        // for the same reason.
        if (clampedTapIndex != lastTapIndex)
        {
            filterA.reset();
            filterB.reset();
            lastOutput = { 0.0f, 0.0f };
            lastTapIndex = clampedTapIndex;
        }

        // Pole/zero angles live in user-Nyquist space; the filter runs at
        // kOversampleFactor x that rate, so scale angles down to keep the
        // perceived pitch constant.
        const float angleScale = 1.0f / static_cast<float> (kOversampleFactor);
        const C pA1 = std::polar (rA1p, tA1p * angleScale);
        const C pA2 = lockedA ? std::conj (pA1) : std::polar (rA2p, tA2p * angleScale);
        const C zA1 = std::polar (rA1z, tA1z * angleScale);
        const C zA2 = lockedA ? std::conj (zA1) : std::polar (rA2z, tA2z * angleScale);
        const C pB1 = std::polar (rB1p, tB1p * angleScale);
        const C pB2 = lockedB ? std::conj (pB1) : std::polar (rB2p, tB2p * angleScale);
        const C zB1 = std::polar (rB1z, tB1z * angleScale);
        const C zB2 = lockedB ? std::conj (zB1) : std::polar (rB2z, tB2z * angleScale);

        // Auto-normalise against the combined linear response at omega=pi/2
        // so the user's gain control stays meaningful as poles move. We
        // normalise the *routed* combination, not each stage individually:
        // series multiplies the two responses, sum/diff add/subtract them.
        auto stageResponse = [] (C p1, C p2, C z1, C z2, C ePos) noexcept
        {
            const C num = (1.0f - z1 * ePos) * (1.0f - z2 * ePos);
            const C den = (1.0f - p1 * ePos) * (1.0f - p2 * ePos);
            return num / den;
        };

        auto combinedMagnitudeAt = [&] (float omega) noexcept
        {
            const C ePos = std::polar (1.0f, -omega);
            const C hA = stageResponse (pA1, pA2, zA1, zA2, ePos);
            const C hB = stageResponse (pB1, pB2, zB1, zB2, ePos);
            switch (routing)
            {
                case Routing::Series:     return std::abs (hA * hB);
                case Routing::Sum:        return std::abs (hA + hB);
                case Routing::Difference: return std::abs (hA - hB);
                default:                  return std::abs (hA);
            }
        };

        const float normRef = juce::jmax (1.0e-6f,
            combinedMagnitudeAt (juce::MathConstants<float>::halfPi));
        const float gainLin = 1.0f / normRef;

        // Each stage runs with unit linear gain; gainLin is applied to the
        // combined output below so the BC level remains in "stage-internal
        // signal units" — i.e. the same units the user dialled in.
        filterA.setCoefficients (pA1, pA2, zA1, zA2, 1.0f);
        filterB.setCoefficients (pB1, pB2, zB1, zB2, 1.0f);
        filterA.setBoundary (bc, bLevel, tap);
        filterB.setBoundary (bc, bLevel, tap);

        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = oversampler->processSamplesUp (block);

        const int upNumSamples  = static_cast<int> (upBlock.getNumSamples());
        const int upNumChannels = static_cast<int> (upBlock.getNumChannels());

        const int probeMask = kProbeDecimation - 1; // kProbeDecimation is a power of two
        static_assert ((kProbeDecimation & (kProbeDecimation - 1)) == 0,
                       "kProbeDecimation must be a power of two");

        auto routeSample = [&] (C x) noexcept -> C
        {
            const C yA = filterA.processSample (x);
            // In Series, stage B sees stage A's BC-shaped output; in the
            // parallel modes, both stages independently see the input.
            const C yB = (routing == Routing::Series)
                            ? filterB.processSample (yA)
                            : filterB.processSample (x);
            switch (routing)
            {
                case Routing::Series:     return yB * gainLin;
                case Routing::Sum:        return (yA + yB) * gainLin;
                case Routing::Difference: return (yA - yB) * gainLin;
                default:                  return yA * gainLin;
            }
        };

        if (upNumChannels >= 2)
        {
            auto* L = upBlock.getChannelPointer (0);
            auto* R = upBlock.getChannelPointer (1);
            for (int n = 0; n < upNumSamples; ++n)
            {
                const C y = routeSample (C { L[n], R[n] } + feedback * lastOutput);
                lastOutput = y;
                L[n] = y.real();
                R[n] = y.imag();
                if ((probeCounter++ & probeMask) == 0)
                {
                    probeA.push (filterA.getState1());
                    probeB.push (filterB.getState1());
                }
            }
        }
        else if (upNumChannels == 1)
        {
            auto* M = upBlock.getChannelPointer (0);
            for (int n = 0; n < upNumSamples; ++n)
            {
                const C y = routeSample (C { M[n], 0.0f } + feedback * lastOutput);
                lastOutput = y;
                M[n] = y.real();
                if ((probeCounter++ & probeMask) == 0)
                {
                    probeA.push (filterA.getState1());
                    probeB.push (filterB.getState1());
                }
            }
        }

        oversampler->processSamplesDown (block);

        // Pure post-everything output trim — doesn't change the BC character.
        const float outputLin = juce::Decibels::decibelsToGain (outputDb);
        if (outputLin != 1.0f)
            buffer.applyGain (outputLin);

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
