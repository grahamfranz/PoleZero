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

#include "ZPlaneComponent.h"
#include "PluginProcessor.h"

#include <cmath>
#include <complex>

namespace polezero
{
    namespace
    {
        // Param IDs + handle identities + colour palette for one filter stage.
        // Two const instances of this (kStageA / kStageB) drive paint(),
        // hitTest(), writeFromMouse() and updateTrail() so the two-stage
        // behaviour stays in one place.
        struct StageDesc
        {
            const char* p1Radius;
            const char* p1Angle;
            const char* p2Radius;
            const char* p2Angle;
            const char* z1Radius;
            const char* z1Angle;
            const char* z2Radius;
            const char* z2Angle;
            const char* lock;
            ZPlaneComponent::Handle hPole1;
            ZPlaneComponent::Handle hPole2;
            ZPlaneComponent::Handle hZero1;
            ZPlaneComponent::Handle hZero2;
            juce::Colour poleStable;   // r < ~0.9
            juce::Colour poleRunaway;  // r >= 1.0
            juce::Colour zeroColour;
            juce::Colour trailColour;
        };

        // CVD-safe palette: stage A is the WARM family (orange / yellow /
        // peach) and stage B is the COOL family (cyan / ice / violet).
        // Avoids the red/green opposition that breaks down under
        // protanopia or deuteranopia. The runaway tint is also a *brightness*
        // shift, not just a hue shift, so it stays visible to anyone.
        const StageDesc kStageA {
            PoleZeroProcessor::kPole1Radius, PoleZeroProcessor::kPole1Angle,
            PoleZeroProcessor::kPole2Radius, PoleZeroProcessor::kPole2Angle,
            PoleZeroProcessor::kZero1Radius, PoleZeroProcessor::kZero1Angle,
            PoleZeroProcessor::kZero2Radius, PoleZeroProcessor::kZero2Angle,
            PoleZeroProcessor::kLockConjugate,
            ZPlaneComponent::Handle::PoleA1, ZPlaneComponent::Handle::PoleA2,
            ZPlaneComponent::Handle::ZeroA1, ZPlaneComponent::Handle::ZeroA2,
            juce::Colour (0xffff9933), juce::Colour (0xfffff04d),  // orange → bright yellow
            juce::Colour (0xffffd180),                              // amber zeros
            juce::Colour (0xffffb070)                               // peach trail
        };

        const StageDesc kStageB {
            PoleZeroProcessor::kPoleB1Radius, PoleZeroProcessor::kPoleB1Angle,
            PoleZeroProcessor::kPoleB2Radius, PoleZeroProcessor::kPoleB2Angle,
            PoleZeroProcessor::kZeroB1Radius, PoleZeroProcessor::kZeroB1Angle,
            PoleZeroProcessor::kZeroB2Radius, PoleZeroProcessor::kZeroB2Angle,
            PoleZeroProcessor::kLockConjugateB,
            ZPlaneComponent::Handle::PoleB1, ZPlaneComponent::Handle::PoleB2,
            ZPlaneComponent::Handle::ZeroB1, ZPlaneComponent::Handle::ZeroB2,
            juce::Colour (0xff4dc8e8), juce::Colour (0xffd6f5ff),  // cyan → pale ice
            juce::Colour (0xffc49eff),                              // lavender zeros
            juce::Colour (0xffd6a0ff)                               // violet trail
        };
    }

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

        // Re-allocate the trail buffer at the new size and clear it. Any
        // previous trail would be at the wrong scale anyway.
        const int w = juce::jmax (1, (int) plotArea.getWidth());
        const int h = juce::jmax (1, (int) plotArea.getHeight());
        trailImage = juce::Image (juce::Image::ARGB, w, h, true);
    }

    juce::Point<float> ZPlaneComponent::complexToView (float r, float theta) const
    {
        const float rView = radiusToView (r);
        const float x = rView * std::cos (theta);
        const float y = rView * std::sin (theta);
        const auto centre = plotArea.getCentre();
        return { centre.x + x * pxPerUnit, centre.y - y * pxPerUnit };
    }

    juce::Point<float> ZPlaneComponent::complexToView (std::complex<float> z) const
    {
        return complexToView (std::abs (z), std::arg (z));
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

    bool ZPlaneComponent::isLockedA() const
    {
        return processor.apvts.getRawParameterValue (PoleZeroProcessor::kLockConjugate)->load() > 0.5f;
    }

    bool ZPlaneComponent::isLockedB() const
    {
        return processor.apvts.getRawParameterValue (PoleZeroProcessor::kLockConjugateB)->load() > 0.5f;
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

        // Phase-portrait trail — drawn under the markers so handles stay
        // legible even when the orbit passes through them.
        if (trailImage.isValid())
            g.drawImageAt (trailImage, (int) plotArea.getX(), (int) plotArea.getY());

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

        auto drawStage = [&] (const StageDesc& s, bool locked)
        {
            const float r1p = processor.apvts.getRawParameterValue (s.p1Radius)->load();
            const float t1p = processor.apvts.getRawParameterValue (s.p1Angle)->load();
            const float r2p = processor.apvts.getRawParameterValue (s.p2Radius)->load();
            const float t2p = processor.apvts.getRawParameterValue (s.p2Angle)->load();
            const float r1z = processor.apvts.getRawParameterValue (s.z1Radius)->load();
            const float t1z = processor.apvts.getRawParameterValue (s.z1Angle)->load();
            const float r2z = processor.apvts.getRawParameterValue (s.z2Radius)->load();
            const float t2z = processor.apvts.getRawParameterValue (s.z2Angle)->load();

            // Locked mode draws the secondary at conj(primary), matching DSP.
            const float r2pDraw = locked ? r1p : r2p;
            const float t2pDraw = locked ? -t1p : t2p;
            const float r2zDraw = locked ? r1z : r2z;
            const float t2zDraw = locked ? -t1z : t2z;

            auto poleColourAt = [&] (float r)
            {
                const float runaway = juce::jlimit (0.0f, 1.0f, (r - 0.9f) / 0.2f);
                return s.poleStable.interpolatedWith (s.poleRunaway, runaway);
            };

            const auto p1Pt = complexToView (r1p, t1p);
            const auto p2Pt = complexToView (r2pDraw, t2pDraw);
            const auto z1Pt = complexToView (r1z, t1z);
            const auto z2Pt = complexToView (r2zDraw, t2zDraw);

            const float secAlpha = locked ? 0.55f : 1.0f;
            const float secSize  = locked ? 7.0f : 9.0f;
            const float secThick = locked ? 2.0f : 2.5f;

            drawCross  (p1Pt, poleColourAt (r1p),                          9.0f,    2.5f);
            drawCross  (p2Pt, poleColourAt (r2pDraw).withAlpha (secAlpha), secSize, secThick);
            drawCircle (z1Pt, s.zeroColour,                                9.0f,    2.5f);
            drawCircle (z2Pt, s.zeroColour.withAlpha (secAlpha),           secSize, secThick);
        };

        drawStage (kStageA, isLockedA());
        drawStage (kStageB, isLockedB());
    }

    ZPlaneComponent::Handle ZPlaneComponent::hitTest (juce::Point<float> p) const
    {
        auto collectStage = [&] (const StageDesc& s, bool locked,
                                 std::array<std::pair<Handle, juce::Point<float>>, 4>& out)
        {
            const float r1p = processor.apvts.getRawParameterValue (s.p1Radius)->load();
            const float t1p = processor.apvts.getRawParameterValue (s.p1Angle)->load();
            const float r2p = processor.apvts.getRawParameterValue (s.p2Radius)->load();
            const float t2p = processor.apvts.getRawParameterValue (s.p2Angle)->load();
            const float r1z = processor.apvts.getRawParameterValue (s.z1Radius)->load();
            const float t1z = processor.apvts.getRawParameterValue (s.z1Angle)->load();
            const float r2z = processor.apvts.getRawParameterValue (s.z2Radius)->load();
            const float t2z = processor.apvts.getRawParameterValue (s.z2Angle)->load();

            const float r2pH = locked ? r1p : r2p;
            const float t2pH = locked ? -t1p : t2p;
            const float r2zH = locked ? r1z : r2z;
            const float t2zH = locked ? -t1z : t2z;

            out = {{
                { s.hPole1, complexToView (r1p, t1p) },
                { s.hPole2, complexToView (r2pH, t2pH) },
                { s.hZero1, complexToView (r1z, t1z) },
                { s.hZero2, complexToView (r2zH, t2zH) }
            }};
        };

        std::array<std::pair<Handle, juce::Point<float>>, 4> handlesA, handlesB;
        collectStage (kStageA, isLockedA(), handlesA);
        collectStage (kStageB, isLockedB(), handlesB);

        Handle best = Handle::None;
        float bestDist = kHitRadiusPx;
        auto consider = [&] (const std::array<std::pair<Handle, juce::Point<float>>, 4>& arr)
        {
            for (const auto& [h, pt] : arr)
            {
                const float d = pt.getDistanceFrom (p);
                if (d <= bestDist) { bestDist = d; best = h; }
            }
        };
        consider (handlesA);
        consider (handlesB);
        return best;
    }

    void ZPlaneComponent::writeFromMouse (Handle h, juce::Point<float> p)
    {
        auto [r, theta] = viewToComplex (p);

        // Locked mode: dragging the secondary handle is equivalent to
        // dragging the primary at the reflected angle.
        if (h == Handle::PoleA2 && isLockedA()) { h = Handle::PoleA1; theta = -theta; }
        if (h == Handle::ZeroA2 && isLockedA()) { h = Handle::ZeroA1; theta = -theta; }
        if (h == Handle::PoleB2 && isLockedB()) { h = Handle::PoleB1; theta = -theta; }
        if (h == Handle::ZeroB2 && isLockedB()) { h = Handle::ZeroB1; theta = -theta; }

        const char* rId     = nullptr;
        const char* thetaId = nullptr;
        switch (h)
        {
            case Handle::PoleA1: rId = PoleZeroProcessor::kPole1Radius;  thetaId = PoleZeroProcessor::kPole1Angle;  break;
            case Handle::PoleA2: rId = PoleZeroProcessor::kPole2Radius;  thetaId = PoleZeroProcessor::kPole2Angle;  break;
            case Handle::ZeroA1: rId = PoleZeroProcessor::kZero1Radius;  thetaId = PoleZeroProcessor::kZero1Angle;  break;
            case Handle::ZeroA2: rId = PoleZeroProcessor::kZero2Radius;  thetaId = PoleZeroProcessor::kZero2Angle;  break;
            case Handle::PoleB1: rId = PoleZeroProcessor::kPoleB1Radius; thetaId = PoleZeroProcessor::kPoleB1Angle; break;
            case Handle::PoleB2: rId = PoleZeroProcessor::kPoleB2Radius; thetaId = PoleZeroProcessor::kPoleB2Angle; break;
            case Handle::ZeroB1: rId = PoleZeroProcessor::kZeroB1Radius; thetaId = PoleZeroProcessor::kZeroB1Angle; break;
            case Handle::ZeroB2: rId = PoleZeroProcessor::kZeroB2Radius; thetaId = PoleZeroProcessor::kZeroB2Angle; break;
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

    void ZPlaneComponent::fadeTrailImage()
    {
        if (! trailImage.isValid()) return;

        // Multiplicative alpha fade. Per-frame factor ~0.92 gives ~0.4 s
        // persistence at 30 Hz. Direct pixel walk because Graphics-based
        // composites can't easily *reduce* alpha — they can only add on top.
        juce::Image::BitmapData bd (trailImage,
                                    juce::Image::BitmapData::readWrite);
        constexpr int kFadeMul = 235; // 235/256 ≈ 0.918 per frame
        for (int y = 0; y < bd.height; ++y)
        {
            auto* line = bd.getLinePointer (y);
            for (int x = 0; x < bd.width; ++x)
            {
                auto* px = reinterpret_cast<juce::PixelARGB*> (
                    line + x * bd.pixelStride);
                px->multiplyAlpha (kFadeMul);
            }
        }
    }

    void ZPlaneComponent::stampTrail (TrajectoryProbe& probe, juce::Colour col)
    {
        if (! trailImage.isValid()) return;

        // Drain the probe into the scratch buffer. The probe is lossy on
        // overflow (it skips ahead to the newest tail), so old samples may
        // be dropped but the most recent ones always make it through.
        const int n = probe.drain (trailScratch.data(), kMaxPointsPerFrame);
        if (n <= 0) return;

        juce::Graphics g (trailImage);
        const float offX = plotArea.getX();
        const float offY = plotArea.getY();

        for (int i = 0; i < n; ++i)
        {
            const auto z = trailScratch[(size_t) i];
            if (! std::isfinite (z.real()) || ! std::isfinite (z.imag())) continue;

            const auto pt = complexToView (z);
            const float lx = pt.x - offX;
            const float ly = pt.y - offY;
            // Bail on points that escaped the view.
            if (lx < 0.0f || ly < 0.0f
                || lx >= (float) trailImage.getWidth()
                || ly >= (float) trailImage.getHeight())
                continue;

            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (lx - 1.0f, ly - 1.0f, 2.0f, 2.0f);
        }
    }

    void ZPlaneComponent::updateTrail()
    {
        fadeTrailImage();
        stampTrail (processor.getTrajectoryProbeA(), kStageA.trailColour);
        stampTrail (processor.getTrajectoryProbeB(), kStageB.trailColour);
    }

    void ZPlaneComponent::timerCallback()
    {
        updateTrail();
        repaint();
    }
}
