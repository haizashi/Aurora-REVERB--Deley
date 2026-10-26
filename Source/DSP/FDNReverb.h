#pragma once
#include "Common.h"
#include "PitchShifter.h"

namespace aurora
{
// 8-line Feedback Delay Network reverb:
// pre-delay -> 3 all-pass diffusers -> 8 modulated delay lines -> Hadamard mixing matrix
// with per-line decay gain (exact RT60) and one-pole HF damping in the loop.
class FDNReverb
{
public:
    static constexpr int N = 8;

    struct Params
    {
        float size = 0.6f;       // 0..1
        float decaySec = 4.0f;   // RT60 in seconds
        float dampHz = 7000.0f;  // HF damping cutoff
        float preMs = 20.0f;
        float mod = 0.35f;       // 0..1 chorus-like modulation of the tail
        float width = 1.0f;      // 0..1.5
        bool freeze = false;
        float shimmer = 0.0f;        // 0..1 amount of pitch-shifted feedback
        float shimSemitones = 12.0f; // interval of the shimmer
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const float msToS = fs * 0.001f;
        for (int i = 0; i < N; ++i)
        {
            lines[i].prepare ((int) (kBaseMs[i] * 2.2f * msToS) + 64);
            lenCur[i] = lenTgt[i] = kBaseMs[i] * msToS;
            phase[i] = (float) i / (float) N;
            lfoInc[i] = (0.11f + 0.037f * (float) i) / fs;
            gain[i] = 0.9f;
        }
        preDelay.prepare ((int) (0.25f * fs) + 4);
        ap[0].prepare ((int) (5.1f * msToS));
        ap[1].prepare ((int) (7.7f * msToS));
        ap[2].prepare ((int) (11.3f * msToS));
        inGain.reset (fs, 0.05f);
        inGain.setCurrentAndTarget (1.0f);
        shifter.prepare (fs);
        shHpCoef = 1.0f - std::exp (-kTwoPi * 150.0f / fs);
        shHp = 0.0f;
        shimAmt.reset (fs, 0.05f);
        shimAmt.setCurrentAndTarget (0.0f);
        shimFeed = 0.0f;
        snap = true;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& a : ap) a.reset();
        preDelay.reset();
        shifter.reset();
        shimFeed = 0.0f;
        shHp = 0.0f;
        for (int i = 0; i < N; ++i) lp[i] = 0.0f;
    }

    void setParams (const Params& p)
    {
        const float sizeScale = 0.6f + 1.4f * clampf (p.size, 0.0f, 1.0f);
        const float rt = std::max (0.05f, p.decaySec);
        for (int i = 0; i < N; ++i)
        {
            lenTgt[i] = kBaseMs[i] * sizeScale * fs * 0.001f;
            const float dSec = lenTgt[i] / fs;
            gain[i] = p.freeze ? 0.9999f : std::pow (10.0f, -3.0f * dSec / rt);
            if (snap) lenCur[i] = lenTgt[i];
        }
        lpCoef = p.freeze ? 1.0f : 1.0f - std::exp (-kTwoPi * clampf (p.dampHz, 500.0f, 0.45f * fs) / fs);
        preTgt = clampf (p.preMs * 0.001f * fs, 1.0f, 0.2f * fs);
        if (snap) preCur = preTgt;
        modDepth = clampf (p.mod, 0.0f, 1.0f) * 0.0005f * fs;
        width = clampf (p.width, 0.0f, 1.5f);
        inGain.setTarget (p.freeze ? 0.0f : 1.0f);
        shifter.setSemitones (p.shimSemitones);
        // shimmer is switched off while frozen: an un-damped loop with pitch-up feedback would run away
        // The shimmer path injects NEW energy into the loop, so what it may add per round trip is
        // limited by the loop's own headroom (1 - g^2). Long decays -> smaller safe amount.
        float g2 = 0.0f;
        for (int i = 0; i < N; ++i) g2 += gain[i] * gain[i];
        g2 /= (float) N;
        const float safe = clampf (kShimHeadroom * std::sqrt (std::max (0.0f, 1.0f - g2)), 0.0f, 1.0f);
        shimAmt.setTarget (p.freeze ? 0.0f : clampf (p.shimmer, 0.0f, 1.0f) * safe);
        snap = false;
    }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float in = 0.5f * (inL + inR);
        preCur += (preTgt - preCur) * 0.001f;
        const float pd = preDelay.read (preCur);
        preDelay.push (in);

        float x = ap[0].process (pd, 0.62f);
        x = ap[1].process (x, 0.60f);
        x = ap[2].process (x, 0.55f);
        x *= inGain.next() * 0.35f;
        x += shimFeed * shimAmt.next() * 0.35f; // pitch-shifted tail re-enters the network

        float z[N];
        for (int i = 0; i < N; ++i)
        {
            lenCur[i] += (lenTgt[i] - lenCur[i]) * 0.0004f;
            phase[i] += lfoInc[i];
            if (phase[i] >= 1.0f) phase[i] -= 1.0f;
            const float m = std::sin (kTwoPi * phase[i]) * modDepth;
            const float y = lines[i].read (clampf (lenCur[i] + m, 2.0f, (float) lines[i].capacity()));
            lp[i] += lpCoef * (y - lp[i]);
            z[i] = lp[i] * gain[i];
        }
        hadamard (z);
        for (int i = 0; i < N; ++i)
            lines[i].push (z[i] + x * kInSign[i]);

        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < N; i += 2)
        {
            l += lp[i] * kOutSignL[i];
            r += lp[i + 1] * kOutSignR[i + 1];
        }
        l *= 0.5f;
        r *= 0.5f;
        // high-pass the shimmer send: DC / sub-bass would otherwise circulate coherently and run away
        const float sIn = 0.5f * (l + r);
        shHp += shHpCoef * (sIn - shHp);
        shimFeed = std::tanh (shifter.process (sIn - shHp));
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
        outL = mid + side;
        outR = mid - side;
    }

private:
    static inline void hadamard (float* v) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }
        for (int i = 0; i < N; ++i) v[i] *= 0.35355339f; // 1/sqrt(8): orthonormal, lossless
    }

    static constexpr float kBaseMs[N]    = { 22.7f, 26.9f, 31.3f, 35.9f, 40.7f, 45.7f, 50.9f, 56.3f };
    static constexpr float kInSign[N]    = { 1, 1, -1, 1, -1, -1, 1, -1 };
    static constexpr float kOutSignL[N]  = { 1, 0, -1, 0, 1, 0, -1, 0 };
    static constexpr float kOutSignR[N]  = { 0, 1, 0, 1, 0, -1, 0, -1 };

    float fs = 48000.0f;
    DelayLine lines[N];
    Allpass ap[3];
    DelayLine preDelay;
    Ramp inGain;
    PitchShifter shifter;
    Ramp shimAmt;
    float shimFeed = 0.0f, shHp = 0.0f, shHpCoef = 0.02f;
    static constexpr float kShimHeadroom = 4.0f; // tuned with the stability test (see tests)
    float lenCur[N] {}, lenTgt[N] {}, lp[N] {}, gain[N] {}, phase[N] {}, lfoInc[N] {};
    float lpCoef = 0.5f, modDepth = 0.0f, width = 1.0f, preCur = 960.0f, preTgt = 960.0f;
    bool snap = true;
};
} // namespace aurora
