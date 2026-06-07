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

        addAndMakeVisible (routingLabel);
        routingLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (routingBox);
        for (int i = 0; i < static_cast<int> (Routing::NumRoutings); ++i)
            routingBox.addItem (routingName (static_cast<Routing> (i)), i + 1);
        routingAttachment = std::make_unique<ComboAttachment> (
            processor.apvts, PoleZeroProcessor::kRouting, routingBox);

        addAndMakeVisible (boundaryLabel);
        boundaryLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (boundaryBox);
        for (int i = 0; i < static_cast<int> (BoundaryCondition::NumBoundaryConditions); ++i)
            boundaryBox.addItem (boundaryConditionName (static_cast<BoundaryCondition> (i)), i + 1);

        boundaryAttachment = std::make_unique<ComboAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundary, boundaryBox);

        addAndMakeVisible (boundaryTapLabel);
        boundaryTapLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (boundaryTapBox);
        for (int i = 0; i < static_cast<int> (BoundaryTap::NumBoundaryTaps); ++i)
            boundaryTapBox.addItem (boundaryTapName (static_cast<BoundaryTap> (i)), i + 1);
        boundaryTapAttachment = std::make_unique<ComboAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundaryTap, boundaryTapBox);

        // Stage A controls
        addAndMakeVisible (stageALabel);
        stageALabel.setJustificationType (juce::Justification::centredLeft);
        stageALabel.setColour (juce::Label::textColourId, juce::Colour (0xffff9933));

        addAndMakeVisible (lockConjugateButton);
        lockConjugateAttachment = std::make_unique<ButtonAttachment> (
            processor.apvts, PoleZeroProcessor::kLockConjugate, lockConjugateButton);

        // Stage B controls
        addAndMakeVisible (stageBLabel);
        stageBLabel.setJustificationType (juce::Justification::centredLeft);
        stageBLabel.setColour (juce::Label::textColourId, juce::Colour (0xff4dc8e8));

        addAndMakeVisible (lockConjugateButtonB);
        lockConjugateBAttachment = std::make_unique<ButtonAttachment> (
            processor.apvts, PoleZeroProcessor::kLockConjugateB, lockConjugateButtonB);

        auto setupRotary = [&] (juce::Slider& s, juce::Label& l)
        {
            addAndMakeVisible (l);
            l.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (s);
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        };

        setupRotary (boundaryLevelSlider, boundaryLevelLabel);
        setupRotary (feedbackSlider,      feedbackLabel);
        setupRotary (outputSlider,        outputLabel);

        boundaryLevelAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kBoundaryLevel, boundaryLevelSlider);
        feedbackAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kFeedback, feedbackSlider);
        outputAttachment = std::make_unique<SliderAttachment> (
            processor.apvts, PoleZeroProcessor::kOutputDb, outputSlider);

        // SliderAttachment installs a textFromValueFunction that calls the
        // parameter's default formatter (no fixed precision -> ~7 digits).
        // Override it here so the text box stays readable.
        boundaryLevelSlider.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
        feedbackSlider.textFromValueFunction      = [] (double v) { return juce::String (v, 2); };
        outputSlider.textFromValueFunction        = [] (double v) { return juce::String (v, 1) + " dB"; };
        boundaryLevelSlider.updateText();
        feedbackSlider.updateText();
        outputSlider.updateText();

        setResizable (true, true);
        setResizeLimits (620, 500, 1800, 1400);
        setSize (820, 620);
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

        const int knobH    = 28;
        const int comboH   = 26;
        const int labelH   = 16;
        const int headerH  = 18;
        const int gap      = 8;

        // Routing first — it determines what the two stages actually do.
        routingLabel.setBounds (controls.removeFromTop (labelH));
        routingBox.setBounds   (controls.removeFromTop (comboH));
        controls.removeFromTop (gap);

        boundaryLabel.setBounds (controls.removeFromTop (labelH));
        boundaryBox.setBounds   (controls.removeFromTop (comboH));
        controls.removeFromTop (gap);

        boundaryTapLabel.setBounds (controls.removeFromTop (labelH));
        boundaryTapBox.setBounds   (controls.removeFromTop (comboH));
        controls.removeFromTop (gap + 4);

        // Stage A block.
        stageALabel.setBounds (controls.removeFromTop (headerH));
        lockConjugateButton.setBounds (controls.removeFromTop (knobH));
        controls.removeFromTop (gap + 4);

        // Stage B block.
        stageBLabel.setBounds (controls.removeFromTop (headerH));
        lockConjugateButtonB.setBounds (controls.removeFromTop (knobH));
        controls.removeFromTop (gap * 2);

        // Three rotaries: Level | Feedback | Output
        const int rotaryRowH = 88;
        auto rotaries = controls.removeFromTop (rotaryRowH);
        const int third = rotaries.getWidth() / 3;
        auto rLevel    = rotaries.removeFromLeft (third);
        auto rFeedback = rotaries.removeFromLeft (third);
        auto rOutput   = rotaries;

        boundaryLevelLabel.setBounds  (rLevel.removeFromTop (labelH));
        boundaryLevelSlider.setBounds (rLevel);

        feedbackLabel.setBounds  (rFeedback.removeFromTop (labelH));
        feedbackSlider.setBounds (rFeedback);

        outputLabel.setBounds  (rOutput.removeFromTop (labelH));
        outputSlider.setBounds (rOutput);
    }
}
