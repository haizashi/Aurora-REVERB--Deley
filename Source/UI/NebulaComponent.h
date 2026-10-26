#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "AuroraLookAndFeel.h"

// Animated "nebula": a glowing core that breathes with the wet signal,
// ripples that fly out on every echo / reflection and orbiting dust particles.
class NebulaComponent : public juce::Component, private juce::Timer
{
public:
    explicit NebulaComponent (AuroraProcessor& p) : proc (p)
    {
        for (int i = 0; i < 48; ++i)
            parts.push_back ({ rng.nextFloat() * juce::MathConstants<float>::twoPi,
                               0.18f + rng.nextFloat() * 0.78f,
                               (0.08f + rng.nextFloat() * 0.35f) * (rng.nextBool() ? 1.0f : -1.0f),
                               0.8f + rng.nextFloat() * 2.4f,
                               rng.nextFloat() });
        for (int i = 0; i < 42; ++i)
            stars.push_back ({ rng.nextFloat(), rng.nextFloat(), rng.nextFloat() });
        shimParam = proc.apvts.getRawParameterValue ("rv_shim");
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }
    ~NebulaComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

        juce::Path clip;
        clip.addRoundedRectangle (b, 18.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1040), c.x, c.y, juce::Colour (0xff06050d), b.getX(), b.getY(), true));
        g.fillRect (b);

        // orbit rings
        for (int k = 1; k <= 4; ++k)
        {
            const float rr = R * 0.25f * (float) k;
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.drawEllipse (c.x - rr * 1.25f, c.y - rr * 0.8f, rr * 2.5f, rr * 1.6f, 1.0f);
        }

        const float v = juce::jlimit (0.0f, 1.0f, lvl);
        const float breath = 1.0f + 0.05f * std::sin (t * 1.6f);
        const float coreR = R * (0.20f + 0.26f * v) * breath;
        const auto coreCol = juce::Colour::fromHSV (0.52f + 0.33f * v, 0.75f, 1.0f, 1.0f);

        // outer halo
        g.setGradientFill (juce::ColourGradient (coreCol.withAlpha (0.35f + 0.3f * v), c.x, c.y,
                                                 coreCol.withAlpha (0.0f), c.x + coreR * 3.2f, c.y, true));
        g.fillEllipse (c.x - coreR * 3.2f, c.y - coreR * 3.2f, coreR * 6.4f, coreR * 6.4f);

        // ripples
        for (const auto& r : ripples)
        {
            const float rr = R * r.r;
            g.setColour (juce::Colour::fromHSV (0.5f + 0.35f * r.hue, 0.7f, 1.0f, r.a * 0.8f));
            g.drawEllipse (c.x - rr, c.y - rr * 0.7f, rr * 2.0f, rr * 1.4f, 1.5f);
        }

        // particles
        for (const auto& p : parts)
        {
            const float wob = 1.0f + 0.08f * std::sin (t * 1.2f + p.hue * 6.0f);
            const float px = c.x + std::cos (p.ang) * p.rad * R * 1.05f * wob;
            const float py = c.y + std::sin (p.ang) * p.rad * R * 0.68f * wob;
            g.setColour (juce::Colour::fromHSV (0.5f + 0.35f * p.hue, 0.6f, 1.0f, 0.25f + 0.55f * v));
            g.fillEllipse (px - p.size * 0.5f, py - p.size * 0.5f, p.size, p.size);
        }

        // shimmer sparkles: twinkling stars that drift upward, more of them the more shimmer is dialled in
        const float shim = shimParam != nullptr ? shimParam->load() * 0.01f : 0.0f;
        if (shim > 0.01f)
            for (const auto& st : stars)
            {
                const float tw = 0.5f + 0.5f * std::sin (t * 2.6f + st.seed * 20.0f);
                float yy = std::fmod (st.y * b.getHeight() - t * (6.0f + 22.0f * st.seed) * (0.3f + shim), b.getHeight());
                if (yy < 0.0f) yy += b.getHeight();
                const float px = b.getX() + st.x * b.getWidth(), py = b.getY() + yy;
                const float sz = 1.5f + 2.5f * tw * shim, al = shim * tw;
                g.setColour (juce::Colours::white.withAlpha (al * 0.9f));
                g.fillEllipse (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
                g.setColour (juce::Colour::fromHSV (0.5f + 0.3f * st.seed, 0.4f, 1.0f, al * 0.55f));
                g.drawLine (px - sz * 1.8f, py, px + sz * 1.8f, py, 0.8f);
                g.drawLine (px, py - sz * 1.8f, px, py + sz * 1.8f, 0.8f);
            }

        // core
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.95f), c.x, c.y,
                                                 coreCol.withAlpha (0.0f), c.x + coreR, c.y, true));
        g.fillEllipse (c.x - coreR, c.y - coreR, coreR * 2.0f, coreR * 2.0f);

        g.setColour (ui::kViolet.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);
    }

private:
    struct Particle { float ang, rad, speed, size, hue; };
    struct Ripple   { float r, a, hue; };
    struct Star     { float x, y, seed; };

    void timerCallback() override
    {
        t += 1.0f / 30.0f;
        const float in = std::sqrt (juce::jlimit (0.0f, 1.0f, proc.getWetLevel() * 1.5f));
        if (in - prev > 0.08f && ripples.size() < 10)
            ripples.push_back ({ 0.16f, 0.8f, rng.nextFloat() });
        prev = in;
        lvl += (in - lvl) * (in > lvl ? 0.5f : 0.08f);

        for (auto& r : ripples) { r.r += 0.010f + 0.010f * lvl; r.a *= 0.955f; }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(),
                                       [] (const Ripple& r) { return r.a < 0.03f || r.r > 1.1f; }),
                       ripples.end());
        for (auto& p : parts) p.ang += p.speed * (0.012f + 0.03f * lvl);
        repaint();
    }

    AuroraProcessor& proc;
    std::vector<Particle> parts;
    std::vector<Ripple> ripples;
    std::vector<Star> stars;
    std::atomic<float>* shimParam = nullptr;
    juce::Random rng;
    float lvl = 0.0f, prev = 0.0f, t = 0.0f;
};
