#include "ZPlaneComponent.h"
#include "PluginProcessor.h"

#include <cmath>

namespace polezero
{
    ZPlaneComponent::ZPlaneComponent (PoleZeroProcessor& proc) : processor (proc)
    {
        startTimerHz (30);
    }

    ZPlaneComponent::~ZPlaneComponent() = default;

    void ZPlaneComponent::resized()
    {
        const auto bounds = getLocalBounds().toFloat();
        const float side = juce::jmin (bounds.getWidth(), bounds.getHeight());
        plotArea = juce::Rectangle<float> (side, side).withCentre (bounds.getCentre());
        pxPerUnit = (plotArea.getWidth() * 0.5f) / kViewExtent;
    }

    juce::Point<float> ZPlaneComponent::complexToView (float r, float theta) const
    {
        const float x = r * std::cos (theta);
        const float y = r * std::sin (theta);
        const auto centre = plotArea.getCentre();
        return { centre.x + x * pxPerUnit, centre.y - y * pxPerUnit };
    }

    std::pair<float, float> ZPlaneComponent::viewToComplex (juce::Point<float> p) const
    {
        const auto centre = plotArea.getCentre();
        const float x = (p.x - centre.x) / pxPerUnit;
        const float y = (centre.y - p.y) / pxPerUnit;
        float r = std::sqrt (x * x + y * y);
        float theta = std::atan2 (y, x);
        // The processor mirrors via the conjugate, so we only expose the upper
        // half-plane. Reflect any drag below the real axis back up.
        if (theta < 0.0f) theta = -theta;
        r     = juce::jlimit (0.0f, PoleZeroProcessor::kRadiusMax, r);
        theta = juce::jlimit (0.0f, juce::MathConstants<float>::pi, theta);
        return { r, theta };
    }

    void ZPlaneComponent::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff111418));

        // Plot frame
        g.setColour (juce::Colour (0xff1d2329));
        g.fillRoundedRectangle (plotArea, 6.0f);

        const auto centre = plotArea.getCentre();
        const float unitR = pxPerUnit; // |z| = 1

        // Unit circle — the stability boundary for the linear filter.
        g.setColour (juce::Colour (0xff5a8db8));
        g.drawEllipse (centre.x - unitR, centre.y - unitR, 2.0f * unitR, 2.0f * unitR, 1.5f);

        // Axes
        g.setColour (juce::Colour (0x55ffffff));
        g.drawLine (plotArea.getX(), centre.y, plotArea.getRight(), centre.y, 1.0f);
        g.drawLine (centre.x, plotArea.getY(), centre.x, plotArea.getBottom(), 1.0f);

        // Labels
        g.setColour (juce::Colour (0x99ffffff));
        g.setFont (11.0f);
        g.drawText ("Re", plotArea.getRight() - 28, (int) centre.y + 4, 26, 14, juce::Justification::right);
        g.drawText ("Im", (int) centre.x + 4, plotArea.getY() + 2, 26, 14, juce::Justification::left);

        const float rP     = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPoleRadius)->load();
        const float thetaP = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPoleAngle)->load();
        const float rZ     = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZeroRadius)->load();
        const float thetaZ = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZeroAngle)->load();

        auto drawCross = [&] (juce::Point<float> p, juce::Colour c, float size, float thickness)
        {
            g.setColour (c);
            g.drawLine (p.x - size, p.y - size, p.x + size, p.y + size, thickness);
            g.drawLine (p.x - size, p.y + size, p.x + size, p.y - size, thickness);
        };

        auto drawCircle = [&] (juce::Point<float> p, juce::Colour c, float size, float thickness)
        {
            g.setColour (c);
            g.drawEllipse (p.x - size, p.y - size, 2.0f * size, 2.0f * size, thickness);
        };

        // Pole colour shifts toward orange as the pole approaches / passes the
        // unit circle to flag the linear-instability region where the BC bites.
        const float runaway = juce::jlimit (0.0f, 1.0f, (rP - 0.9f) / 0.2f);
        const juce::Colour poleColour = juce::Colour (0xffff6b6b).interpolatedWith (
            juce::Colour (0xffffaa33), runaway);

        const auto pole     = complexToView (rP, thetaP);
        const auto poleConj = complexToView (rP, -thetaP);
        const auto zero     = complexToView (rZ, thetaZ);
        const auto zeroConj = complexToView (rZ, -thetaZ);

        drawCross (pole,     poleColour,                       9.0f, 2.5f);
        drawCross (poleConj, poleColour.withAlpha (0.55f),     7.0f, 2.0f);

        const juce::Colour zeroColour (0xff7bc8ff);
        drawCircle (zero,     zeroColour,                     9.0f, 2.5f);
        drawCircle (zeroConj, zeroColour.withAlpha (0.55f),   7.0f, 2.0f);
    }

    ZPlaneComponent::Handle ZPlaneComponent::hitTest (juce::Point<float> p) const
    {
        const float rP     = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPoleRadius)->load();
        const float thetaP = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPoleAngle)->load();
        const float rZ     = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZeroRadius)->load();
        const float thetaZ = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZeroAngle)->load();

        const auto polePt = complexToView (rP, thetaP);
        const auto zeroPt = complexToView (rZ, thetaZ);

        const float dPole = polePt.getDistanceFrom (p);
        const float dZero = zeroPt.getDistanceFrom (p);

        if (dPole > kHitRadiusPx && dZero > kHitRadiusPx) return Handle::None;
        return dPole <= dZero ? Handle::Pole : Handle::Zero;
    }

    void ZPlaneComponent::writeFromMouse (Handle h, juce::Point<float> p)
    {
        const auto [r, theta] = viewToComplex (p);
        const char* rId     = (h == Handle::Pole) ? PoleZeroProcessor::kPoleRadius : PoleZeroProcessor::kZeroRadius;
        const char* thetaId = (h == Handle::Pole) ? PoleZeroProcessor::kPoleAngle  : PoleZeroProcessor::kZeroAngle;

        if (auto* pr = processor.apvts.getParameter (rId))
            pr->setValueNotifyingHost (pr->convertTo0to1 (r));
        if (auto* pt = processor.apvts.getParameter (thetaId))
            pt->setValueNotifyingHost (pt->convertTo0to1 (theta));
    }

    void ZPlaneComponent::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        dragging = hitTest (p);
        if (dragging != Handle::None)
            writeFromMouse (dragging, p);
    }

    void ZPlaneComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging == Handle::None) return;
        writeFromMouse (dragging, e.position);
    }

    void ZPlaneComponent::timerCallback()
    {
        repaint();
    }
}
