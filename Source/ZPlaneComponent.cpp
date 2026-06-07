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

    float ZPlaneComponent::radiusToView (float r)
    {
        return r <= 1.0f ? r : 1.0f + kOutsideCompression * (r - 1.0f);
    }

    float ZPlaneComponent::viewToRadius (float v)
    {
        return v <= 1.0f ? v : 1.0f + (v - 1.0f) / kOutsideCompression;
    }

    void ZPlaneComponent::resized()
    {
        const auto bounds = getLocalBounds().toFloat();
        const float side = juce::jmin (bounds.getWidth(), bounds.getHeight());
        plotArea = juce::Rectangle<float> (side, side).withCentre (bounds.getCentre());
        pxPerUnit = (plotArea.getWidth() * 0.5f) / kViewExtent;
    }

    juce::Point<float> ZPlaneComponent::complexToView (float r, float theta) const
    {
        const float rView = radiusToView (r);
        const float x = rView * std::cos (theta);
        const float y = rView * std::sin (theta);
        const auto centre = plotArea.getCentre();
        return { centre.x + x * pxPerUnit, centre.y - y * pxPerUnit };
    }

    std::pair<float, float> ZPlaneComponent::viewToComplex (juce::Point<float> p) const
    {
        const auto centre = plotArea.getCentre();
        const float x = (p.x - centre.x) / pxPerUnit;
        const float y = (centre.y - p.y) / pxPerUnit;
        const float vR = juce::jlimit (0.0f, kViewExtent, std::sqrt (x * x + y * y));
        const float theta = std::atan2 (y, x); // in [-pi, pi]
        const float r = juce::jlimit (0.0f, PoleZeroProcessor::kRadiusMax, viewToRadius (vR));
        return { r, theta };
    }

    bool ZPlaneComponent::isLocked() const
    {
        return processor.apvts.getRawParameterValue (PoleZeroProcessor::kLockConjugate)->load() > 0.5f;
    }

    void ZPlaneComponent::paint (juce::Graphics& g)
    {
        // Single uniform background. The handles can reach the plotArea edge
        // (view radius = kViewExtent), so a separate plot-area fill would
        // leave the cross/circle markers straddling a colour seam.
        g.fillAll (juce::Colour (0xff111418));

        const auto centre = plotArea.getCentre();
        const float unitR = pxPerUnit;

        g.setColour (juce::Colour (0xff5a8db8));
        g.drawEllipse (centre.x - unitR, centre.y - unitR, 2.0f * unitR, 2.0f * unitR, 1.5f);

        g.setColour (juce::Colour (0x55ffffff));
        g.drawLine (plotArea.getX(), centre.y, plotArea.getRight(), centre.y, 1.0f);
        g.drawLine (centre.x, plotArea.getY(), centre.x, plotArea.getBottom(), 1.0f);

        g.setColour (juce::Colour (0x99ffffff));
        g.setFont (11.0f);
        g.drawText ("Re", plotArea.getRight() - 28, (int) centre.y + 4, 26, 14, juce::Justification::right);
        g.drawText ("Im", (int) centre.x + 4, plotArea.getY() + 2, 26, 14, juce::Justification::left);

        const float r1p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole1Radius)->load();
        const float t1p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole1Angle)->load();
        const float r2p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole2Radius)->load();
        const float t2p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole2Angle)->load();
        const float r1z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero1Radius)->load();
        const float t1z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero1Angle)->load();
        const float r2z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero2Radius)->load();
        const float t2z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero2Angle)->load();

        const bool locked = isLocked();

        // In locked mode the secondary handle draws at the conjugate of the
        // primary, regardless of its stored value — matches what the DSP uses.
        const float r2pDraw = locked ? r1p : r2p;
        const float t2pDraw = locked ? -t1p : t2p;
        const float r2zDraw = locked ? r1z : r2z;
        const float t2zDraw = locked ? -t1z : t2z;

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

        auto poleColourAt = [] (float r)
        {
            const float runaway = juce::jlimit (0.0f, 1.0f, (r - 0.9f) / 0.2f);
            return juce::Colour (0xffff6b6b).interpolatedWith (juce::Colour (0xffffaa33), runaway);
        };

        const juce::Colour zeroColour (0xff7bc8ff);

        const auto p1Pt = complexToView (r1p, t1p);
        const auto p2Pt = complexToView (r2pDraw, t2pDraw);
        const auto z1Pt = complexToView (r1z, t1z);
        const auto z2Pt = complexToView (r2zDraw, t2zDraw);

        // Primary handles are always bright. Secondary handles are ghosted in
        // locked mode (slaves) and full-bright in unlocked mode (independent).
        const float secAlpha    = locked ? 0.55f : 1.0f;
        const float secSize     = locked ? 7.0f : 9.0f;
        const float secThick    = locked ? 2.0f : 2.5f;

        drawCross  (p1Pt, poleColourAt (r1p),                       9.0f,    2.5f);
        drawCross  (p2Pt, poleColourAt (r2pDraw).withAlpha (secAlpha), secSize, secThick);
        drawCircle (z1Pt, zeroColour,                               9.0f,    2.5f);
        drawCircle (z2Pt, zeroColour.withAlpha (secAlpha),          secSize, secThick);
    }

    ZPlaneComponent::Handle ZPlaneComponent::hitTest (juce::Point<float> p) const
    {
        const float r1p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole1Radius)->load();
        const float t1p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole1Angle)->load();
        const float r2p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole2Radius)->load();
        const float t2p = processor.apvts.getRawParameterValue (PoleZeroProcessor::kPole2Angle)->load();
        const float r1z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero1Radius)->load();
        const float t1z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero1Angle)->load();
        const float r2z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero2Radius)->load();
        const float t2z = processor.apvts.getRawParameterValue (PoleZeroProcessor::kZero2Angle)->load();

        const bool locked = isLocked();
        const float r2pH = locked ? r1p : r2p;
        const float t2pH = locked ? -t1p : t2p;
        const float r2zH = locked ? r1z : r2z;
        const float t2zH = locked ? -t1z : t2z;

        const std::pair<Handle, juce::Point<float>> handles[] = {
            { Handle::Pole1, complexToView (r1p, t1p) },
            { Handle::Pole2, complexToView (r2pH, t2pH) },
            { Handle::Zero1, complexToView (r1z, t1z) },
            { Handle::Zero2, complexToView (r2zH, t2zH) },
        };

        Handle best = Handle::None;
        float bestDist = kHitRadiusPx;
        for (const auto& [h, pt] : handles)
        {
            const float d = pt.getDistanceFrom (p);
            if (d <= bestDist)
            {
                bestDist = d;
                best = h;
            }
        }
        return best;
    }

    void ZPlaneComponent::writeFromMouse (Handle h, juce::Point<float> p)
    {
        auto [r, theta] = viewToComplex (p);

        const bool locked = isLocked();
        if (locked)
        {
            // Dragging a secondary handle in locked mode is just dragging the
            // primary at the reflected position.
            if (h == Handle::Pole2) { h = Handle::Pole1; theta = -theta; }
            if (h == Handle::Zero2) { h = Handle::Zero1; theta = -theta; }
        }

        const char* rId     = nullptr;
        const char* thetaId = nullptr;
        switch (h)
        {
            case Handle::Pole1: rId = PoleZeroProcessor::kPole1Radius; thetaId = PoleZeroProcessor::kPole1Angle; break;
            case Handle::Pole2: rId = PoleZeroProcessor::kPole2Radius; thetaId = PoleZeroProcessor::kPole2Angle; break;
            case Handle::Zero1: rId = PoleZeroProcessor::kZero1Radius; thetaId = PoleZeroProcessor::kZero1Angle; break;
            case Handle::Zero2: rId = PoleZeroProcessor::kZero2Radius; thetaId = PoleZeroProcessor::kZero2Angle; break;
            default: return;
        }

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
