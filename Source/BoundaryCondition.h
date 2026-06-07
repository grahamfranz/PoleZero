#pragma once

#include <cmath>

namespace polezero
{
    enum class BoundaryCondition
    {
        Clip = 0,
        Foldover,
        Modulo,
        Tanh,
        NumBoundaryConditions
    };

    inline const char* boundaryConditionName (BoundaryCondition bc) noexcept
    {
        switch (bc)
        {
            case BoundaryCondition::Clip:     return "Clip";
            case BoundaryCondition::Foldover: return "Foldover";
            case BoundaryCondition::Modulo:   return "Modulo";
            case BoundaryCondition::Tanh:     return "Tanh";
            default:                          return "?";
        }
    }

    // Where in the filter the boundary condition bites. The same map (Clip /
    // Foldover / Modulo / Tanh) is applied, but at different signal nodes.
    //
    //   Output  shape the output `y = b0*x + z1` before it feeds back into the
    //           state. This is the original PoleZero behaviour and the only
    //           tap that catches runaway when the pole radius >= 1.
    //   State   leave y linear, but shape the next-step state memories z1, z2
    //           before they're written. Also bounds the system, but the
    //           audible output is the un-shaped y — runaway folds inside the
    //           recursion rather than at the listener.
    //   Input   shape x at the filter input; the rest of the biquad runs
    //           linearly. This is just a waveshaper into a linear filter and
    //           will not catch pole runaway — exposed deliberately for A/B.
    enum class BoundaryTap
    {
        Output = 0,
        State,
        Input,
        NumBoundaryTaps
    };

    inline const char* boundaryTapName (BoundaryTap t) noexcept
    {
        switch (t)
        {
            case BoundaryTap::Output: return "Output";
            case BoundaryTap::State:  return "State";
            case BoundaryTap::Input:  return "Input";
            default:                  return "?";
        }
    }

    // Signed boundary map applied to a feedback sample inside the DSP loop.
    // When the filter runs away (e.g. pole radius >= 1) the recursion would
    // otherwise blow up; this squashes the feedback to [-L, L] and the chosen
    // shape determines how the runaway energy folds back into the filter.
    //
    //   Clip      hard clip at +/- L (square self-oscillation at high res)
    //   Foldover  triangle wave, period 4L; classic wavefolder feedback
    //   Modulo    sawtooth wrap, period 2L; bitcrush-style discontinuity
    //   Tanh      soft saturation; Moog-style "warm" runaway
    inline float applyBoundarySigned (float x, float L, BoundaryCondition bc) noexcept
    {
        if (L <= 0.0f) return 0.0f;
        if (! std::isfinite (x)) return 0.0f;

        switch (bc)
        {
            case BoundaryCondition::Clip:
                return x > L ? L : (x < -L ? -L : x);

            case BoundaryCondition::Foldover:
            {
                const float period = 4.0f * L;
                float t = std::fmod (x + L, period);
                if (t < 0.0f) t += period;
                return t < 2.0f * L ? (t - L) : (3.0f * L - t);
            }

            case BoundaryCondition::Modulo:
            {
                const float period = 2.0f * L;
                float t = std::fmod (x + L, period);
                if (t < 0.0f) t += period;
                return t - L;
            }

            case BoundaryCondition::Tanh:
                return L * std::tanh (x / L);

            default:
                return x;
        }
    }
}
