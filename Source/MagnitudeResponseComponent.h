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

#include <juce_gui_basics/juce_gui_basics.h>

namespace polezero
{
    class PoleZeroProcessor;

    // Renders the linear magnitude response |H(e^{jw})| of the current pole/
    // zero configuration on a log-frequency / dB axis. Nonlinear behaviour
    // from the boundary condition is not reflected; this is the underlying
    // linear filter shape.
    class MagnitudeResponseComponent : public juce::Component,
                                       private juce::Timer
    {
    public:
        explicit MagnitudeResponseComponent (PoleZeroProcessor& proc);
        ~MagnitudeResponseComponent() override;

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        PoleZeroProcessor& processor;

        static constexpr int   kNumPoints = 384;
        static constexpr float kDbMax     =  24.0f;
        static constexpr float kDbMin     = -36.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnitudeResponseComponent)
    };
}
