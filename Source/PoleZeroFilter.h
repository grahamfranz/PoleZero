#pragma once

#include <complex>
#include <vector>

#include "BoundaryCondition.h"

namespace polezero
{
    // Complex-arithmetic biquad on a packed L+iR signal. The user gets two
    // independent complex poles (p1, p2) and two independent complex zeros
    // (z1, z2). When p2 = conj(p1) and z2 = conj(z1), the polynomial has
    // real coefficients, the complex recursion decouples into two identical
    // real biquads on L and R, and the per-component boundary condition
    // reproduces the previous scalar BC on each channel bit-for-bit.
    //
    // Drop the conjugate constraint and the filter response becomes asymmetric
    // in frequency, with energy crossing between L and R every sample.
    //
    // The boundary condition is applied to the real and imaginary parts of the
    // feedback sample independently before they re-enter the state update.
    class PoleZeroFilter
    {
    public:
        using C = std::complex<float>;

        void prepare() noexcept { reset(); }

        void reset() noexcept
        {
            state.z1 = C { 0.0f, 0.0f };
            state.z2 = C { 0.0f, 0.0f };
        }

        void setCoefficients (C p1, C p2, C z1, C z2, float gainLinear) noexcept
        {
            b0 = C { gainLinear, 0.0f };
            b1 = -gainLinear * (z1 + z2);
            b2 =  gainLinear * (z1 * z2);
            a1 = -(p1 + p2);
            a2 =  p1 * p2;
        }

        void setBoundary (BoundaryCondition newBc, float newLevel) noexcept
        {
            bc = newBc;
            level = newLevel;
        }

        C processSample (C x) noexcept
        {
            const C yRaw = b0 * x + state.z1;
            const C y { applyBoundarySigned (yRaw.real(), level, bc),
                        applyBoundarySigned (yRaw.imag(), level, bc) };
            state.z1 = b1 * x - a1 * y + state.z2;
            state.z2 = b2 * x - a2 * y;
            return y;
        }

    private:
        struct State { C z1 { 0.0f, 0.0f }; C z2 { 0.0f, 0.0f }; };
        State state;

        C b0 { 1.0f, 0.0f }, b1 { 0.0f, 0.0f }, b2 { 0.0f, 0.0f };
        C a1 { 0.0f, 0.0f }, a2 { 0.0f, 0.0f };

        BoundaryCondition bc { BoundaryCondition::Tanh };
        float level { 1.0f };
    };
}
