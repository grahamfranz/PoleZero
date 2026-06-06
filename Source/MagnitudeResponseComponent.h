#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace polezero
{
    class PoleZeroProcessor;

    // Renders the linear magnitude response |H(e^{jw})| of the current pole/
    // zero configuration on a log-frequency / dB axis. Nonlinear behaviour
    // from the boundary condition is not reflected; this is the underlying
    // linear filter shape.
    class MagnitudeResponseComponent : public juce::Component,
                                       private juce::Timer
    {
    public:
        explicit MagnitudeResponseComponent (PoleZeroProcessor& proc);
        ~MagnitudeResponseComponent() override;

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        PoleZeroProcessor& processor;

        static constexpr int   kNumPoints = 384;
        static constexpr float kDbMax     =  24.0f;
        static constexpr float kDbMin     = -36.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnitudeResponseComponent)
    };
}
