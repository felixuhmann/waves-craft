// Craft Audio doubler - per-voice triangle LFO (spec r1 section 7, review decision D3).
#pragma once

namespace craft::doubler
{

// Unit triangle: 0 at phase 0, rising to +1 at 1/4, -1 at 3/4, back to 0 at 1.
// The voice multiplies it by the signed depth, so a negative depth starts falling.
class TriangleLfo
{
public:
    void reset() noexcept { phase = 0.0; }
    void setFrequency (double hz, double sampleRate) noexcept { increment = hz / sampleRate; }
    double getPhase() const noexcept { return phase; }

    static double shape (double p) noexcept
    {
        if (p < 0.25) return 4.0 * p;
        if (p < 0.75) return 2.0 - 4.0 * p;
        return 4.0 * p - 4.0;
    }

    // value at the current phase, then advance by one sample
    double next() noexcept
    {
        const double v = shape (phase);
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
        return v;
    }

private:
    double phase = 0.0, increment = 0.0;
};

} // namespace craft::doubler
