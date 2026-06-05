#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BoundaryCondition.h"

namespace polezero
{
    PoleZeroEditor::PoleZeroEditor (PoleZeroProcessor& p)
        : juce::AudioProcessorEditor (&p), processor (p), zPlane (p)
    {
        addAndMakeVisible (zPlane);

        addAndMakeVisible (boundaryLabel);
        boundaryLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (boundaryBox);
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryBox.addItem (boundaryConditionName (static_cast<BoundaryCondition> (i)), i + 1);

        boundaryAttachment = std::make_unique<ComboAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundary, boundaryBox);

        addAndMakeVisible (boundaryLevelLabel);
        boundaryLevelLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (boundaryLevelSlider);
        boundaryLevelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        boundaryLevelSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        boundaryLevelAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundaryLevel, boundaryLevelSlider);

        addAndMakeVisible (gainLabel);
        gainLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (gainSlider);
        gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        gainAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kGainDb, gainSlider);

        setResizable (true, true);
        setResizeLimits (520, 460, 1600, 1200);
        setSize (720, 600);
    }

    void PoleZeroEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0c0f12));

        g.setColour (juce::Colour (0x99ffffff));
        g.setFont (13.0f);
        g.drawText ("PoleZero — drag the pole (x) and zero (o) on the z-plane",
                    getLocalBounds().removeFromTop (24).reduced (14, 4),
                    juce::Justification::centredLeft);
    }

    void PoleZeroEditor::resized()
    {
        auto bounds = getLocalBounds().reduced (12);
        bounds.removeFromTop (20); // header text

        auto controls = bounds.removeFromBottom (136);
        zPlane.setBounds (bounds);

        controls.removeFromTop (8);

        auto row1 = controls.removeFromTop (28);
        boundaryLabel.setBounds (row1.removeFromLeft (90));
        boundaryBox.setBounds   (row1.removeFromLeft (180));

        controls.removeFromTop (8);
        auto row2 = controls.removeFromTop (28);
        boundaryLevelLabel.setBounds  (row2.removeFromLeft (90));
        boundaryLevelSlider.setBounds (row2);

        controls.removeFromTop (8);
        auto row3 = controls.removeFromTop (28);
        gainLabel.setBounds  (row3.removeFromLeft (90));
        gainSlider.setBounds (row3);
    }
}
