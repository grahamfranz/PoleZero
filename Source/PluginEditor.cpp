#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BoundaryCondition.h"

namespace polezero
{
    PoleZeroEditor::PoleZeroEditor (PoleZeroProcessor& p)
        : juce::AudioProcessorEditor (&p), processor (p), zPlane (p)
    {
        addAndMakeVisible (zPlane);

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

        setupRotary (driveSlider,         driveLabel);
        setupRotary (boundaryLevelSlider, boundaryLevelLabel);
        setupRotary (outputSlider,        outputLabel);

        driveAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kDriveDb, driveSlider);
        boundaryLevelAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundaryLevel, boundaryLevelSlider);
        outputAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kOutputDb, outputSlider);

        setResizable (true, true);
        setResizeLimits (560, 420, 1800, 1200);
        setSize (760, 520);
    }

    void PoleZeroEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0c0f12));

        g.setColour (juce::Colour (0x99ffffff));
        g.setFont (13.0f);
        g.drawText ("PoleZero",
                    getLocalBounds().removeFromTop (24).reduced (14, 4),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colour (0x55ffffff));
        g.setFont (11.0f);
        g.drawText ("pole angle = oscillation pitch  ·  drag along the unit circle",
                    getLocalBounds().removeFromTop (24).reduced (14, 4),
                    juce::Justification::centredRight);
    }

    void PoleZeroEditor::resized()
    {
        auto bounds = getLocalBounds().reduced (12);
        bounds.removeFromTop (22); // header strip

        const int controlsWidth = 220;
        auto controls = bounds.removeFromRight (controlsWidth);
        controls.removeFromLeft (12); // gap

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

        // Three rotaries side-by-side: Drive | Level | Output
        const int rotaryRowH = 88;
        auto rotaries = controls.removeFromTop (rotaryRowH);
        const int third = rotaries.getWidth() / 3;
        auto rDrive  = rotaries.removeFromLeft (third);
        auto rLevel  = rotaries.removeFromLeft (third);
        auto rOutput = rotaries;

        driveLabel.setBounds  (rDrive.removeFromTop (labelH));
        driveSlider.setBounds (rDrive);

        boundaryLevelLabel.setBounds  (rLevel.removeFromTop (labelH));
        boundaryLevelSlider.setBounds (rLevel);

        outputLabel.setBounds  (rOutput.removeFromTop (labelH));
        outputSlider.setBounds (rOutput);
    }
}
