#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
        explicit ZPlaneComponent (PoleZeroProcessor& proc);
        ~ZPlaneComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;

    private:
        enum class Handle { None, Pole1, Pole2, Zero1, Zero2 };

        void timerCallback() override;
        juce::Point<float> complexToView (float r, float theta) const;
        std::pair<float, float> viewToComplex (juce::Point<float> p) const;
        Handle hitTest (juce::Point<float> p) const;
        void writeFromMouse (Handle h, juce::Point<float> p);
        bool isLocked() const;

        PoleZeroProcessor& processor;
        Handle dragging { Handle::None };
        juce::Rectangle<float> plotArea;
        float pxPerUnit { 1.0f };

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
