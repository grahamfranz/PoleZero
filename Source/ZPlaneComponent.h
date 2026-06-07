#pragma once

#include <array>
#include <complex>

#include <juce_gui_basics/juce_gui_basics.h>

#include "TrajectoryProbe.h"

namespace polezero
{
    class PoleZeroProcessor;

    // Interactive z-plane view. The user drags the pole (drawn as 'x') and the
    // zero (drawn as 'o'); their conjugates are mirrored automatically. When
    // the pole sits at or outside the unit circle the marker glows orange to
    // signal that the linear filter is unstable and the boundary condition is
    // the thing keeping the output bounded.
    class ZPlaneComponent : public juce::Component,
                            private juce::Timer
    {
    public:
        // Public so the file-scope StageDesc table in ZPlaneComponent.cpp
        // can name handles by value. Not part of any external contract.
        enum class Handle
        {
            None,
            PoleA1, PoleA2, ZeroA1, ZeroA2,
            PoleB1, PoleB2, ZeroB1, ZeroB2
        };

        explicit ZPlaneComponent (PoleZeroProcessor& proc);
        ~ZPlaneComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;

    private:

        void timerCallback() override;
        juce::Point<float> complexToView (float r, float theta) const;
        juce::Point<float> complexToView (std::complex<float> z) const;
        std::pair<float, float> viewToComplex (juce::Point<float> p) const;
        Handle hitTest (juce::Point<float> p) const;
        void writeFromMouse (Handle h, juce::Point<float> p);
        bool isLockedA() const;
        bool isLockedB() const;
        void updateTrail();
        void fadeTrailImage();
        void stampTrail (TrajectoryProbe& probe, juce::Colour col);

        PoleZeroProcessor& processor;
        Handle dragging { Handle::None };
        juce::Rectangle<float> plotArea;
        float pxPerUnit { 1.0f };

        // Phase-portrait overlay. trailImage is the same size as plotArea; on
        // each timer tick we fade it slightly and stamp newly-arrived state
        // samples drained from the two TrajectoryProbes (one per stage). The
        // image is painted under the pole/zero markers so handles stay
        // legible even when an orbit passes through them.
        juce::Image trailImage;
        static constexpr int kMaxPointsPerFrame = 2048;
        std::array<std::complex<float>, kMaxPointsPerFrame> trailScratch {};

        // Piecewise radial mapping: inside the unit circle is 1:1, outside is
        // compressed by kOutsideCompression. Reclaims most of the plot for the
        // region where pole/zero placement actually changes the character.
        static constexpr float kOutsideCompression = 0.4f;
        static float radiusToView (float r);
        static float viewToRadius (float v);

        // View half-extent in remapped z-plane units; covers radius 0 .. 1.6.
        static constexpr float kViewExtent = 1.0f + kOutsideCompression * (1.6f - 1.0f);
        static constexpr float kHitRadiusPx = 12.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZPlaneComponent)
    };
}
