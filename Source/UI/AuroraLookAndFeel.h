#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
inline const juce::Colour kCyan    = juce::Colour (0xff22e6ff);
inline const juce::Colour kMagenta = juce::Colour (0xffff4fd8);
inline const juce::Colour kViolet  = juce::Colour (0xffa879ff);
inline const juce::Colour kText    = juce::Colour (0xfff4f1ff);
inline const juce::Colour kDim     = juce::Colour (0xffa9a3c7);

inline juce::Font makeFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height).withStyle (bold ? "Bold" : "Regular"));
}

class AuroraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuroraLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x22ffffff));
        setColour (juce::ComboBox::outlineColourId,    juce::Colour (0x44ffffff));
        setColour (juce::ComboBox::textColourId,       kText);
        setColour (juce::ComboBox::arrowColourId,      kViolet);
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff140c2b));
        setColour (juce::PopupMenu::textColourId,                  kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a2a7a));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const bool dimmed = ! s.isEnabled();
        if (dimmed) g.beginTransparencyLayer (0.35f);
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float arcR = radius - 3.0f, thick = 3.5f;

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0x26ffffff));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc (bipolar knobs start from the centre) with neon glow
        float from = startAngle;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            from = startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle);
        const float a0 = juce::jmin (from, angle), a1 = juce::jmax (from, angle);
        if (a1 - a0 > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.06f * (float) (5 - i)));
                g.strokePath (val, juce::PathStrokeType (thick + (float) i * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body
        const float br = arcR - 8.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2f66), c.x - br * 0.4f, c.y - br * 0.6f,
                                                 juce::Colour (0xff0c0919), c.x + br * 0.5f, c.y + br * 0.8f, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

        // pointer
        const float sn = std::sin (angle), cs = std::cos (angle);
        const juce::Point<float> p0 (c.x + sn * br * 0.35f, c.y - cs * br * 0.35f);
        const juce::Point<float> p1 (c.x + sn * br * 0.88f, c.y - cs * br * 0.88f);
        g.setColour (accent);
        g.drawLine (juce::Line<float> (p0, p1), 2.5f);
        if (dimmed) g.endTransparencyLayer();
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (3.0f);
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        const bool on = b.getToggleState();
        const float cr = r.getHeight() * 0.5f;

        if (on)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.07f * (float) (4 - i)));
                g.drawRoundedRectangle (r.expanded ((float) i), cr + (float) i, 2.0f);
            }
            g.setColour (accent.withAlpha (0.22f));
            g.fillRoundedRectangle (r, cr);
        }
        else if (hover)
        {
            g.setColour (juce::Colour (0x12ffffff));
            g.fillRoundedRectangle (r, cr);
        }

        g.setColour (on ? accent : juce::Colour (0x55ffffff));
        g.drawRoundedRectangle (r, cr, 1.4f);

        const juce::Rectangle<float> led (r.getX() + 10.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour (on ? accent : juce::Colour (0x44ffffff));
        g.fillEllipse (led);

        g.setColour (on ? juce::Colours::white : juce::Colour (0xffb9b3d6));
        g.setFont (makeFont (12.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (28.0f), juce::Justification::centredLeft, true);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (makeFont (12.0f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
};
} // namespace ui
