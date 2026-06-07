#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BoundaryCondition.h"

namespace polezero
{
    PoleZeroEditor::PoleZeroEditor (PoleZeroProcessor& p)
        : juce::AudioProcessorEditor (&p), processor (p), zPlane (p), magnitudeView (p)
    {
        addAndMakeVisible (zPlane);
        addAndMakeVisible (magnitudeView);

        addAndMakeVisible (lockConjugateButton);
        lockConjugateAttachment = std::make_unique<ButtonAttachment> (
            processor.apvts, PoleZeroProcessor::kLockConjugate, lockConjugateButton);

        addAndMakeVisible (boundaryLabel);
        boundaryLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (boundaryBox);
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryBox.addItem (boundaryConditionName (static_cast<BoundaryCondition> (i)), i + 1);

        boundaryAttachment = std::make_unique<ComboAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundary, boundaryBox);

        auto setupRotary = [&] (juce::Slider& s, juce::Label& l)
        {
            addAndMakeVisible (l);
            l.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (s);
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        };

        setupRotary (boundaryLevelSlider, boundaryLevelLabel);
        setupRotary (outputSlider,        outputLabel);

        boundaryLevelAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundaryLevel, boundaryLevelSlider);
        outputAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kOutputDb, outputSlider);

        // SliderAttachment installs a textFromValueFunction that calls the
        // parameter's default formatter (no fixed precision -> ~7 digits).
        // Override it here so the text box stays readable.
        boundaryLevelSlider.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
        outputSlider.textFromValueFunction        = [] (double v) { return juce::String (v, 1) + " dB"; };
        boundaryLevelSlider.updateText();
        outputSlider.updateText();

        setResizable (true, true);
        setResizeLimits (560, 440, 1800, 1200);
        setSize (760, 560);
    }

    void PoleZeroEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0c0f12));

        g.setColour (juce::Colour (0x66ffffff));
        g.setFont (13.0f);
        g.drawText ("PoleZero",
                    getLocalBounds().removeFromTop (22).reduced (14, 4),
                    juce::Justification::centredLeft);
    }

    void PoleZeroEditor::resized()
    {
        auto bounds = getLocalBounds().reduced (12);
        bounds.removeFromTop (18); // header

        // Magnitude curve sits beneath everything as a slim readout strip.
        const int magnitudeStripH = 96;
        auto magArea = bounds.removeFromBottom (magnitudeStripH);
        bounds.removeFromBottom (6);
        magnitudeView.setBounds (magArea);

        const int controlsWidth = 220;
        auto controls = bounds.removeFromRight (controlsWidth);
        controls.removeFromLeft (12);

        zPlane.setBounds (bounds);

        const int knobH   = 28;
        const int comboH  = 26;
        const int labelH  = 16;
        const int gap     = 10;

        lockConjugateButton.setBounds (controls.removeFromTop (knobH));
        controls.removeFromTop (gap * 2);

        boundaryLabel.setJustificationType (juce::Justification::centredLeft);
        boundaryLabel.setBounds (controls.removeFromTop (labelH));
        boundaryBox.setBounds   (controls.removeFromTop (comboH));
        controls.removeFromTop (gap * 2);

        // Two rotaries: Level | Output
        const int rotaryRowH = 88;
        auto rotaries = controls.removeFromTop (rotaryRowH);
        const int half = rotaries.getWidth() / 2;
        auto rLevel  = rotaries.removeFromLeft (half);
        auto rOutput = rotaries;

        boundaryLevelLabel.setBounds  (rLevel.removeFromTop (labelH));
        boundaryLevelSlider.setBounds (rLevel);

        outputLabel.setBounds  (rOutput.removeFromTop (labelH));
        outputSlider.setBounds (rOutput);
    }
}
