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
#include <juce_gui_basics/juce_gui_basics.h>

#include "MagnitudeResponseComponent.h"
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

        ZPlaneComponent             zPlane;
        MagnitudeResponseComponent  magnitudeView;

        juce::ComboBox routingBox;
        juce::Label    routingLabel { {}, "Routing" };

        juce::ComboBox boundaryBox;
        juce::Label    boundaryLabel { {}, "Boundary" };

        juce::ComboBox boundaryTapBox;
        juce::Label    boundaryTapLabel { {}, "Tap" };

        juce::Label    stageALabel { {}, "Stage A" };
        juce::ToggleButton lockConjugateButton  { "Lock Conjugate" };

        juce::Label    stageBLabel { {}, "Stage B" };
        juce::ToggleButton lockConjugateButtonB { "Lock Conjugate" };

        juce::Slider boundaryLevelSlider;
        juce::Label  boundaryLevelLabel { {}, "Level" };

        juce::Slider feedbackSlider;
        juce::Label  feedbackLabel { {}, "Feedback" };

        juce::Slider outputSlider;
        juce::Label  outputLabel { {}, "Output" };

        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
        using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        std::unique_ptr<ButtonAttachment> lockConjugateAttachment;
        std::unique_ptr<ButtonAttachment> lockConjugateBAttachment;
        std::unique_ptr<ComboAttachment>  routingAttachment;
        std::unique_ptr<ComboAttachment>  boundaryAttachment;
        std::unique_ptr<ComboAttachment>  boundaryTapAttachment;
        std::unique_ptr<SliderAttachment> boundaryLevelAttachment;
        std::unique_ptr<SliderAttachment> feedbackAttachment;
        std::unique_ptr<SliderAttachment> outputAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoleZeroEditor)
    };
}
