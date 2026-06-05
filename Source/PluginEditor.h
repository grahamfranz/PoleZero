#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ZPlaneComponent.h"

namespace polezero
{
    class PoleZeroProcessor;

    class PoleZeroEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit PoleZeroEditor (PoleZeroProcessor&);
        ~PoleZeroEditor() override = default;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        PoleZeroProcessor& processor;

        ZPlaneComponent zPlane;

        juce::ToggleButton lockConjugateButton { "Lock Conjugate" };

        juce::ComboBox boundaryBox;
        juce::Label    boundaryLabel { {}, "Boundary" };

        juce::Slider boundaryLevelSlider;
        juce::Label  boundaryLevelLabel { {}, "Level" };

        juce::Slider gainSlider;
        juce::Label  gainLabel { {}, "Gain (dB)" };

        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
        using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        std::unique_ptr<ButtonAttachment> lockConjugateAttachment;
        std::unique_ptr<ComboAttachment>  boundaryAttachment;
        std::unique_ptr<SliderAttachment> boundaryLevelAttachment;
        std::unique_ptr<SliderAttachment> gainAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoleZeroEditor)
    };
}
