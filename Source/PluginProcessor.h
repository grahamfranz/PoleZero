/*
 * PoleZero — a nonlinear z-plane filter plugin.
 * Copyright (C) 2026 Graham Franz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "BoundaryCondition.h"
#include "PoleZeroFilter.h"
#include "TrajectoryProbe.h"

namespace polezero
{
    // How the two stages combine. Series gives a 4-pole / 4-zero cascade
    // (B applied to A's output). Sum and Difference are the parallel
    // routings: A and B each see the input, and we add or subtract their
    // outputs at the combine point — Difference is the Hordijk twin-peak
    // trick where the notch between two close resonances becomes the voice.
    enum class Routing
    {
        Series = 0,
        Sum,
        Difference,
        NumRoutings
    };

    inline const char* routingName (Routing r) noexcept
    {
        switch (r)
        {
            case Routing::Series:     return "Series";
            case Routing::Sum:        return "Sum";
            case Routing::Difference: return "Difference";
            default:                  return "?";
        }
    }

    class PoleZeroProcessor : public juce::AudioProcessor
    {
    public:
        PoleZeroProcessor();
        ~PoleZeroProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "PoleZero"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        juce::AudioProcessorValueTreeState apvts;

        // Parameter IDs (also used by the editor). Stage A keeps the
        // original "pole1/zero1/..." IDs so existing presets load unchanged
        // with stage B at identity (all radii = 0).
        static constexpr const char* kPole1Radius     = "pole1Radius";
        static constexpr const char* kPole1Angle      = "pole1Angle";
        static constexpr const char* kPole2Radius     = "pole2Radius";
        static constexpr const char* kPole2Angle      = "pole2Angle";
        static constexpr const char* kZero1Radius     = "zero1Radius";
        static constexpr const char* kZero1Angle      = "zero1Angle";
        static constexpr const char* kZero2Radius     = "zero2Radius";
        static constexpr const char* kZero2Angle      = "zero2Angle";
        static constexpr const char* kLockConjugate   = "lockConjugate";
        static constexpr const char* kBoundaryTap     = "boundaryTap";

        static constexpr const char* kPoleB1Radius    = "poleB1Radius";
        static constexpr const char* kPoleB1Angle     = "poleB1Angle";
        static constexpr const char* kPoleB2Radius    = "poleB2Radius";
        static constexpr const char* kPoleB2Angle     = "poleB2Angle";
        static constexpr const char* kZeroB1Radius    = "zeroB1Radius";
        static constexpr const char* kZeroB1Angle     = "zeroB1Angle";
        static constexpr const char* kZeroB2Radius    = "zeroB2Radius";
        static constexpr const char* kZeroB2Angle     = "zeroB2Angle";
        static constexpr const char* kLockConjugateB  = "lockConjugateB";

        static constexpr const char* kRouting         = "routing";
        static constexpr const char* kBoundary        = "boundary";
        static constexpr const char* kBoundaryLevel   = "boundaryLevel";
        static constexpr const char* kFeedback        = "feedback";
        static constexpr const char* kOutputDb        = "outputDb";

        // Maximum radius reachable from the GUI / parameter ranges. The poles
        // can sit at or outside the unit circle; the in-DSP boundary condition
        // is what keeps the filter from diverging.
        static constexpr float kRadiusMax = 1.6f;

        // 4x internal oversampling (2 polyphase IIR stages) tames the
        // harmonics the boundary condition kicks up over Nyquist.
        static constexpr int kOversampleStages = 2;
        static constexpr int kOversampleFactor = 1 << kOversampleStages;

        // Decimation between filter state samples pushed to the UI's
        // trajectory display. The internal rate is fs * kOversampleFactor,
        // so at 48 kHz this still gives 24 kHz of state samples — far above
        // anything a 30 Hz repaint can show, but generous for the lossy
        // probe's "newest tail" behaviour.
        static constexpr int kProbeDecimation = 8;

        TrajectoryProbe& getTrajectoryProbeA() noexcept { return probeA; }
        TrajectoryProbe& getTrajectoryProbeB() noexcept { return probeB; }

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

        PoleZeroFilter filterA;
        PoleZeroFilter filterB;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        TrajectoryProbe probeA;
        TrajectoryProbe probeB;
        int probeCounter { 0 };
        int lastTapIndex { static_cast<int> (BoundaryTap::Output) };

        // One-sample memory of the routed output, mixed back into the input
        // by the Feedback parameter. With BC tap on Output the in-stage BC
        // catches any divergence past unity feedback gain.
        std::complex<float> lastOutput { 0.0f, 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoleZeroProcessor)
    };
}
