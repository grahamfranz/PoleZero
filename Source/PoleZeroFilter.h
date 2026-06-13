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

#pragma once

#include <cmath>
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

        void setBoundary (BoundaryCondition newBc, float newLevel, BoundaryTap newTap) noexcept
        {
            bc = newBc;
            level = newLevel;
            tap = newTap;
        }

        C getState1() const noexcept { return state.z1; }

        C processSample (C x) noexcept
        {
            auto shape = [&] (C v) noexcept -> C
            {
                return { applyBoundarySigned (v.real(), level, bc),
                         applyBoundarySigned (v.imag(), level, bc) };
            };

            const C yRaw = b0 * x + state.z1;
            C y = (tap == BoundaryTap::Output) ? shape (yRaw) : yRaw;

            C s1 = b1 * x - a1 * y + state.z2;
            C s2 = b2 * x - a2 * y;

            if (tap == BoundaryTap::State)
            {
                s1 = shape (s1);
                s2 = shape (s2);
            }

            // Either Output or State tap places the BC inside the recursion,
            // so y / s1 / s2 stay bounded across samples. This guard still
            // catches the rare case of a non-finite sample arriving from the
            // host (denormals or NaN in the bus).
            if (! std::isfinite (s1.real()) || ! std::isfinite (s1.imag())) s1 = C { 0.0f, 0.0f };
            if (! std::isfinite (s2.real()) || ! std::isfinite (s2.imag())) s2 = C { 0.0f, 0.0f };

            state.z1 = s1;
            state.z2 = s2;
            return y;
        }

    private:
        struct State { C z1 { 0.0f, 0.0f }; C z2 { 0.0f, 0.0f }; };
        State state;

        C b0 { 1.0f, 0.0f }, b1 { 0.0f, 0.0f }, b2 { 0.0f, 0.0f };
        C a1 { 0.0f, 0.0f }, a2 { 0.0f, 0.0f };

        BoundaryCondition bc { BoundaryCondition::Tanh };
        float level { 1.0f };
        BoundaryTap tap { BoundaryTap::Output };
    };
}
