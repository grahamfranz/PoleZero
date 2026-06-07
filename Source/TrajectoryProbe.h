#pragma once

#include <array>
#include <atomic>
#include <complex>

namespace polezero
{
    // Single-producer / single-consumer ring buffer of complex state samples.
    // The audio thread pushes the filter's internal state every N input
    // samples; the UI thread drains it on its repaint timer to plot a fading
    // phase-portrait of the filter on the z-plane.
    //
    // The buffer is intentionally lossy: if the reader falls behind by more
    // than the capacity, the writer overwrites old samples and the reader
    // catches up by jumping to the newest tail. A 16k-sample window holds
    // about a third of a second at 48 kHz / decimation 8, which is well over
    // a UI frame even at 30 Hz.
    class TrajectoryProbe
    {
    public:
        using C = std::complex<float>;

        static constexpr int kCapacity = 1 << 14; // 16384, must be power of two
        static_assert ((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of two");

        void reset() noexcept
        {
            writeIdx.store (0, std::memory_order_relaxed);
            readIdx = 0;
        }

        // Audio thread.
        void push (C v) noexcept
        {
            const int w = writeIdx.load (std::memory_order_relaxed);
            buffer[static_cast<size_t> (w & (kCapacity - 1))] = v;
            writeIdx.store (w + 1, std::memory_order_release);
        }

        // UI thread. Drains up to maxCount samples into dest in write order;
        // if the producer has lapped us, skips ahead to the newest maxCount.
        // Returns how many samples were copied.
        int drain (C* dest, int maxCount) noexcept
        {
            const int w = writeIdx.load (std::memory_order_acquire);
            int avail = w - readIdx;
            if (avail <= 0) return 0;

            if (avail > kCapacity)
            {
                // Producer lapped us; skip ahead to the newest window.
                readIdx = w - kCapacity;
                avail = kCapacity;
            }
            if (avail > maxCount)
            {
                // Reader asked for fewer than available; take the newest tail.
                readIdx = w - maxCount;
                avail = maxCount;
            }

            for (int i = 0; i < avail; ++i)
                dest[i] = buffer[static_cast<size_t> ((readIdx + i) & (kCapacity - 1))];
            readIdx += avail;
            return avail;
        }

    private:
        alignas (64) std::array<C, kCapacity> buffer {};
        alignas (64) std::atomic<int> writeIdx { 0 };
        int readIdx { 0 };
    };
}
