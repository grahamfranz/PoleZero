#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "BoundaryCondition.h"
#include "PoleZeroFilter.h"

namespace polezero
{
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

        // Parameter IDs (also used by the editor).
        static constexpr const char* kPole1Radius    = "pole1Radius";
        static constexpr const char* kPole1Angle     = "pole1Angle";
        static constexpr const char* kPole2Radius    = "pole2Radius";
        static constexpr const char* kPole2Angle     = "pole2Angle";
        static constexpr const char* kZero1Radius    = "zero1Radius";
        static constexpr const char* kZero1Angle     = "zero1Angle";
        static constexpr const char* kZero2Radius    = "zero2Radius";
        static constexpr const char* kZero2Angle     = "zero2Angle";
        static constexpr const char* kLockConjugate  = "lockConjugate";
        static constexpr const char* kBoundary       = "boundary";
        static constexpr const char* kBoundaryLevel  = "boundaryLevel";
        static constexpr const char* kGainDb         = "gainDb";

        // Maximum radius reachable from the GUI / parameter ranges. The poles
        // can sit at or outside the unit circle; the in-DSP boundary condition
        // is what keeps the filter from diverging.
        static constexpr float kRadiusMax = 1.6f;

        // 4x internal oversampling (2 polyphase IIR stages) tames the
        // harmonics the boundary condition kicks up over Nyquist.
        static constexpr int kOversampleStages = 2;
        static constexpr int kOversampleFactor = 1 << kOversampleStages;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

        PoleZeroFilter filter;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoleZeroProcessor)
    };
}
