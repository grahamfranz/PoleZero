#pragma once

#include <cmath>
#include <vector>

#include "BoundaryCondition.h"

namespace polezero
{
    // Stereo biquad with a conjugate pole pair and a conjugate zero pair.
    // H(z) = g * (1 - 2 rZ cos(thetaZ) z^-1 + rZ^2 z^-2)
    //          / (1 - 2 rP cos(thetaP) z^-1 + rP^2 z^-2)
    // Direct-form II transposed per channel.
    //
    // The boundary condition is applied to the feedback sample every step.
    // When the pole sits at or outside the unit circle the linear recursion
    // would diverge; the BC squashes the feedback to +/- boundaryLevel before
    // it re-enters the state update, so the runaway energy folds back into
    // the filter according to the chosen shape.
    class PoleZeroFilter
    {
    public:
        void prepare (int numChannels)
        {
            state.assign (static_cast<size_t> (numChannels), { 0.0f, 0.0f });
        }

        void reset() noexcept
        {
            for (auto& s : state) s = { 0.0f, 0.0f };
        }

        void setCoefficients (float rP, float thetaP, float rZ, float thetaZ, float gainLinear) noexcept
        {
            b0 = gainLinear;
            b1 = -2.0f * rZ * std::cos (thetaZ) * gainLinear;
            b2 = (rZ * rZ) * gainLinear;
            a1 = -2.0f * rP * std::cos (thetaP);
            a2 = rP * rP;
        }

        void setBoundary (BoundaryCondition newBc, float newLevel) noexcept
        {
            bc = newBc;
            level = newLevel;
        }

        float processSample (int channel, float x) noexcept
        {
            auto& s = state[static_cast<size_t> (channel)];
            const float yRaw = b0 * x + s.z1;
            const float y    = applyBoundarySigned (yRaw, level, bc);
            s.z1 = b1 * x - a1 * y + s.z2;
            s.z2 = b2 * x - a2 * y;
            return y;
        }

    private:
        struct State { float z1; float z2; };
        std::vector<State> state;

        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
        float a1 { 0.0f }, a2 { 0.0f };

        BoundaryCondition bc { BoundaryCondition::Tanh };
        float level { 1.0f };
    };
}
