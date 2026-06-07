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

        const float r1p = lookup (PoleZeroProcessor::kPole1Radius)->load();
        const float t1p = lookup (PoleZeroProcessor::kPole1Angle)->load();
        const float r2p = lookup (PoleZeroProcessor::kPole2Radius)->load();
        const float t2p = lookup (PoleZeroProcessor::kPole2Angle)->load();
        const float r1z = lookup (PoleZeroProcessor::kZero1Radius)->load();
        const float t1z = lookup (PoleZeroProcessor::kZero1Angle)->load();
        const float r2z = lookup (PoleZeroProcessor::kZero2Radius)->load();
        const float t2z = lookup (PoleZeroProcessor::kZero2Angle)->load();
        const bool  locked = lookup (PoleZeroProcessor::kLockConjugate)->load() > 0.5f;

        const float angleScale = 1.0f / static_cast<float> (PoleZeroProcessor::kOversampleFactor);
        const C p1 = std::polar (r1p, t1p * angleScale);
        const C p2 = locked ? std::conj (p1) : std::polar (r2p, t2p * angleScale);
        const C z1 = std::polar (r1z, t1z * angleScale);
        const C z2 = locked ? std::conj (z1) : std::polar (r2z, t2z * angleScale);

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

        for (int i = 0; i < kNumPoints; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (kNumPoints - 1);
            const float audibleOmega  = audibleMin + t * (audibleMax - audibleMin);
            const float internalOmega = audibleOmega * angleScale;
            const C zinv = std::polar (1.0f, -internalOmega);
            const C num  = (1.0f - z1 * zinv) * (1.0f - z2 * zinv);
            const C den  = (1.0f - p1 * zinv) * (1.0f - p2 * zinv);
            const float mag = std::abs (num / den);
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
