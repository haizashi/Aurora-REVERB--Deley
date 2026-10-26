#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace aurora
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

// Circular delay line with linear interpolation.
// Call read() BEFORE push(): read (d) returns the sample written d samples ago (d >= 1).
struct DelayLine
{
    void prepare (int maxDelaySamples)
    {
        buf.assign ((size_t) maxDelaySamples + 4, 0.0f);
        size = (int) buf.size();
        pos = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    int capacity() const noexcept { return size - 2; }

    inline float read (float d) const noexcept
    {
        float rp = (float) pos - d;
        if (rp < 0.0f) rp += (float) size;
        int i0 = (int) rp;
        const float fr = rp - (float) i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1;
        if (i1 >= size) i1 = 0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
    inline void push (float x) noexcept
    {
        buf[(size_t) pos] = x;
        if (++pos >= size) pos = 0;
    }

    std::vector<float> buf;
    int size = 0, pos = 0;
};

// Schroeder all-pass diffuser
struct Allpass
{
    void prepare (int len) { dl.prepare (len + 2); delay = (float) std::max (1, len); }
    void reset() { dl.reset(); }
    inline float process (float x, float g) noexcept
    {
        const float d = dl.read (delay);
        const float v = x - g * d;
        dl.push (v);
        return d + g * v;
    }
    DelayLine dl;
    float delay = 1.0f;
};

// Linear parameter ramp (anti-zipper)
struct Ramp
{
    void reset (float sampleRate, float seconds) { steps = std::max (1, (int) (sampleRate * seconds)); }
    void setCurrentAndTarget (float v) { cur = tgt = v; step = 0.0f; count = 0; }
    void setTarget (float v)
    {
        if (v == tgt) return;
        tgt = v;
        count = steps;
        step = (tgt - cur) / (float) steps;
    }
    inline float next() noexcept
    {
        if (count > 0) { cur += step; if (--count == 0) cur = tgt; }
        return cur;
    }
    float cur = 0.0f, tgt = 0.0f, step = 0.0f;
    int count = 0, steps = 1;
};
} // namespace aurora
