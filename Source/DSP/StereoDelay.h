#pragma once
#include "Common.h"

namespace aurora
{
// Stereo tape-style delay: ping-pong or dual, low/high cut and saturation in the loop,
// slow wobble (flutter) and smooth time changes (tape pitch glide).
class StereoDelay
{
public:
    struct Params
    {
        float timeMs = 380.0f;
        float feedback = 0.45f;     // 0..0.95
        float lowCutHz = 120.0f;
        float highCutHz = 6000.0f;
        float saturation = 0.25f;   // 0..1
        float wobble = 0.2f;        // 0..1
        float offset = 0.0f;        // -0.5..0.5, right channel time offset
        bool pingPong = true;
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxS = (int) (3.0f * fs) + (int) (0.01f * fs) + 16;
        for (auto& d : dl) d.prepare (maxS);
        time.reset (fs, 0.2f);
        wobInc = 0.65f / fs;
        snap = true;
        reset();
    }

    void reset()
    {
        for (auto& d : dl) d.reset();
        for (int c = 0; c < 2; ++c) lcState[c] = hcState[c] = 0.0f;
        wobPhase = 0.0f;
    }

    void setParams (const Params& np)
    {
        p = np;
        p.feedback = clampf (p.feedback, 0.0f, 0.95f);
        const float t = clampf (p.timeMs, 1.0f, 2000.0f) * 0.001f * fs;
        if (snap) { time.setCurrentAndTarget (t); snap = false; }
        else time.setTarget (t);
        lcCoef = 1.0f - std::exp (-kTwoPi * clampf (p.lowCutHz, 10.0f, 2000.0f) / fs);
        hcCoef = 1.0f - std::exp (-kTwoPi * clampf (p.highCutHz, 200.0f, 0.45f * fs) / fs);
        drive = 1.0f + 2.0f * clampf (p.saturation, 0.0f, 1.0f);
        wobDepth = clampf (p.wobble, 0.0f, 1.0f) * 0.004f * fs;
        offset = clampf (p.offset, -0.5f, 0.5f);
    }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float tL = time.next();
        wobPhase += wobInc;
        if (wobPhase >= 1.0f) wobPhase -= 1.0f;
        const float wl = std::sin (kTwoPi * wobPhase) * wobDepth;
        const float wr = std::cos (kTwoPi * wobPhase) * wobDepth;

        const float tapL = dl[0].read (clampf (tL + wl, 1.0f, (float) dl[0].capacity()));
        const float tapR = dl[1].read (clampf (tL * (1.0f + offset) + wr, 1.0f, (float) dl[1].capacity()));

        const float sl = saturate (tapL), sr = saturate (tapR);
        float xl, xr;
        if (p.pingPong)
        {
            const float m = 0.5f * (inL + inR);
            xl = m + p.feedback * sr;
            xr = p.feedback * sl;
        }
        else
        {
            xl = inL + p.feedback * sl;
            xr = inR + p.feedback * sr;
        }
        dl[0].push (condition (xl, 0));
        dl[1].push (condition (xr, 1));
        outL = tapL;
        outR = tapR;
    }

private:
    inline float saturate (float x) const noexcept
    {
        return p.saturation > 0.001f ? std::tanh (x * drive) / drive : x;
    }
    inline float condition (float x, int ch) noexcept
    {
        lcState[ch] += lcCoef * (x - lcState[ch]);
        const float hp = x - lcState[ch];
        hcState[ch] += hcCoef * (hp - hcState[ch]);
        return hcState[ch];
    }

    float fs = 48000.0f;
    DelayLine dl[2];
    Ramp time;
    Params p;
    float lcCoef = 0.01f, hcCoef = 0.5f, drive = 1.0f, wobDepth = 0.0f, wobInc = 0.0f, wobPhase = 0.0f, offset = 0.0f;
    float lcState[2] {}, hcState[2] {};
    bool snap = true;
};
} // namespace aurora
