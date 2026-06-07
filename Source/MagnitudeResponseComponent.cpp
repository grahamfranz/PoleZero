#include "MagnitudeResponseComponent.h"
#include "PluginProcessor.h"

#include <cmath>
#include <complex>

namespace polezero
{
    MagnitudeResponseComponent::MagnitudeResponseComponent (PoleZeroProcessor& p)
        : processor (p)
    {
        startTimerHz (30);
    }

    MagnitudeResponseComponent::~MagnitudeResponseComponent() = default;

    void MagnitudeResponseComponent::timerCallback() { repaint(); }

    void MagnitudeResponseComponent::paint (juce::Graphics& g)
    {
        using C = std::complex<float>;

        auto bounds = getLocalBounds().toFloat();

        auto lookup = [&] (const char* id) {
            return processor.apvts.getRawParameterValue (id);
        };

        const float rA1p = lookup (PoleZeroProcessor::kPole1Radius)->load();
        const float tA1p = lookup (PoleZeroProcessor::kPole1Angle)->load();
        const float rA2p = lookup (PoleZeroProcessor::kPole2Radius)->load();
        const float tA2p = lookup (PoleZeroProcessor::kPole2Angle)->load();
        const float rA1z = lookup (PoleZeroProcessor::kZero1Radius)->load();
        const float tA1z = lookup (PoleZeroProcessor::kZero1Angle)->load();
        const float rA2z = lookup (PoleZeroProcessor::kZero2Radius)->load();
        const float tA2z = lookup (PoleZeroProcessor::kZero2Angle)->load();
        const bool  lockedA = lookup (PoleZeroProcessor::kLockConjugate)->load() > 0.5f;

        const float rB1p = lookup (PoleZeroProcessor::kPoleB1Radius)->load();
        const float tB1p = lookup (PoleZeroProcessor::kPoleB1Angle)->load();
        const float rB2p = lookup (PoleZeroProcessor::kPoleB2Radius)->load();
        const float tB2p = lookup (PoleZeroProcessor::kPoleB2Angle)->load();
        const float rB1z = lookup (PoleZeroProcessor::kZeroB1Radius)->load();
        const float tB1z = lookup (PoleZeroProcessor::kZeroB1Angle)->load();
        const float rB2z = lookup (PoleZeroProcessor::kZeroB2Radius)->load();
        const float tB2z = lookup (PoleZeroProcessor::kZeroB2Angle)->load();
        const bool  lockedB = lookup (PoleZeroProcessor::kLockConjugateB)->load() > 0.5f;

        const int routingIndex = static_cast<int> (lookup (PoleZeroProcessor::kRouting)->load());
        const auto routing = static_cast<Routing> (juce::jlimit (0,
            static_cast<int> (Routing::NumRoutings) - 1, routingIndex));

        const float angleScale = 1.0f / static_cast<float> (PoleZeroProcessor::kOversampleFactor);
        const C pA1 = std::polar (rA1p, tA1p * angleScale);
        const C pA2 = lockedA ? std::conj (pA1) : std::polar (rA2p, tA2p * angleScale);
        const C zA1 = std::polar (rA1z, tA1z * angleScale);
        const C zA2 = lockedA ? std::conj (zA1) : std::polar (rA2z, tA2z * angleScale);
        const C pB1 = std::polar (rB1p, tB1p * angleScale);
        const C pB2 = lockedB ? std::conj (pB1) : std::polar (rB2p, tB2p * angleScale);
        const C zB1 = std::polar (rB1z, tB1z * angleScale);
        const C zB2 = lockedB ? std::conj (zB1) : std::polar (rB2z, tB2z * angleScale);

        auto mapDb = [&] (float db)
        {
            return bounds.getY() + bounds.getHeight() * (kDbMax - db) / (kDbMax - kDbMin);
        };

        auto mapX = [&] (int i)
        {
            return bounds.getX() + bounds.getWidth() * (static_cast<float> (i)
                                                       / static_cast<float> (kNumPoints - 1));
        };

        // Single 0 dB reference line — keep grid minimal in the Ciat-Lonbarde
        // spirit; the curve shape is the readout.
        g.setColour (juce::Colour (0x22ffffff));
        g.drawLine (bounds.getX(), mapDb (0.0f), bounds.getRight(), mapDb (0.0f), 1.0f);

        // Build the response curve. Linear frequency axis from DC to Nyquist —
        // matches the user's linear-angle drag on the unit circle so a pole at
        // angle theta peaks at horizontal position theta/pi. The filter runs
        // internally at kOversampleFactor x rate so audible omega maps to
        // internal omega * angleScale.
        const float pi = juce::MathConstants<float>::pi;
        const float audibleMin = 0.0f;
        const float audibleMax = pi;

        juce::Path path;
        bool started = false;

        auto stageResp = [] (C p1, C p2, C z1, C z2, C zinv) noexcept
        {
            const C num = (1.0f - z1 * zinv) * (1.0f - z2 * zinv);
            const C den = (1.0f - p1 * zinv) * (1.0f - p2 * zinv);
            return num / den;
        };

        for (int i = 0; i < kNumPoints; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (kNumPoints - 1);
            const float audibleOmega  = audibleMin + t * (audibleMax - audibleMin);
            const float internalOmega = audibleOmega * angleScale;
            const C zinv = std::polar (1.0f, -internalOmega);
            const C hA = stageResp (pA1, pA2, zA1, zA2, zinv);
            const C hB = stageResp (pB1, pB2, zB1, zB2, zinv);
            float mag = 0.0f;
            switch (routing)
            {
                case Routing::Series:     mag = std::abs (hA * hB); break;
                case Routing::Sum:        mag = std::abs (hA + hB); break;
                case Routing::Difference: mag = std::abs (hA - hB); break;
                default:                  mag = std::abs (hA); break;
            }
            float dB = 20.0f * std::log10 (juce::jmax (1.0e-6f, mag));
            dB = juce::jlimit (kDbMin, kDbMax, dB);

            const float x = mapX (i);
            const float y = mapDb (dB);
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else            { path.lineTo (x, y); }
        }

        g.setColour (juce::Colour (0xff7bc8ff));
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }
}
