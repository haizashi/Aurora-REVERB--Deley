#pragma once
#include "Common.h"

namespace aurora
{
// Granular pitch shifter: two read taps sweep through a delay line, crossfaded with
// sin^2 windows (they always sum to 1). ratio > 1 = up, < 1 = down.
class PitchShifter
{
public:
    void prepare (float sampleRate)
    {
        grain = std::max (64.0f, 0.075f * sampleRate);
        buf.prepare ((int) grain + 8);
        phase = 0.0f;
        setSemitones (12.0f);
        reset();
    }
    void reset() { buf.reset(); phase = 0.0f; }
    void setSemitones (float st) { ratio = std::pow (2.0f, st / 12.0f); }

    inline float process (float x) noexcept
    {
        phase += (1.0f - ratio) / grain;
        if (phase < 0.0f) phase += 1.0f;
        else if (phase >= 1.0f) phase -= 1.0f;
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float s1 = std::sin (kPi * phase), s2 = std::sin (kPi * p2);
        const float y = s1 * s1 * buf.read (2.0f + phase * grain)
                      + s2 * s2 * buf.read (2.0f + p2 * grain);
        buf.push (x);
        return y;
    }

private:
    DelayLine buf;
    float grain = 3600.0f, phase = 0.0f, ratio = 2.0f;
};
} // namespace aurora
