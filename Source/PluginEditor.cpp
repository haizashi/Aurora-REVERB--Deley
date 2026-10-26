#include "PluginEditor.h"

namespace
{
constexpr int kW = 1000, kH = 556;

void drawGlow (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour col, float alpha)
{
    g.setGradientFill (juce::ColourGradient (col.withAlpha (alpha), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true));
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                const juce::String& sub, juce::Colour accent)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x1affffff), r.getX(), r.getY(),
                                             juce::Colour (0x08ffffff), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 18.0f);
    g.setColour (accent.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.5f), 18.0f, 1.0f);

    const float dx = r.getX() + 24.0f, dy = r.getY() + 23.0f;
    drawGlow (g, { dx, dy }, 12.0f, accent, 0.55f);
    g.setColour (accent);
    g.fillEllipse (dx - 4.0f, dy - 4.0f, 8.0f, 8.0f);

    g.setColour (ui::kText);
    g.setFont (ui::makeFont (16.0f, true));
    g.drawText (title, juce::Rectangle<float> (dx + 14.0f, r.getY() + 9.0f, 110.0f, 22.0f), juce::Justification::centredLeft);
    g.setColour (ui::kDim);
    g.setFont (ui::makeFont (10.5f));
    g.drawText (sub, juce::Rectangle<float> (dx + 14.0f, r.getY() + 28.0f, 130.0f, 14.0f), juce::Justification::centredLeft);

    // thin separator above the third (utility) row
    g.setColour (accent.withAlpha (0.14f));
    g.drawLine (r.getX() + 18.0f, r.getY() + 50.0f + 216.0f, r.getRight() - 18.0f, r.getY() + 50.0f + 216.0f, 1.0f);
}
} // namespace

//==============================================================================
AuroraEditor::AuroraEditor (AuroraProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), nebula (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (nebula);

    using ui::kCyan; using ui::kMagenta;
    delayKnobs = { addKnob ("dl_time", "TIME",     kCyan), addKnob ("dl_fb",  "FEEDBACK", kCyan),
                   addKnob ("dl_lc",   "LOW CUT",  kCyan), addKnob ("dl_hc",  "HIGH CUT", kCyan),
                   addKnob ("dl_sat",  "SATURATE", kCyan), addKnob ("dl_wob", "WOBBLE",   kCyan),
                   addKnob ("dl_off",  "OFFSET",   kCyan), addKnob ("dl_mix", "MIX",      kCyan) };

    reverbKnobs = { addKnob ("rv_size",  "SIZE",    kMagenta), addKnob ("rv_decay", "DECAY",   kMagenta),
                    addKnob ("rv_damp",  "DAMPING", kMagenta), addKnob ("rv_pre",   "PRE-DLY", kMagenta),
                    addKnob ("rv_mod",   "MOD",     kMagenta), addKnob ("rv_width", "WIDTH",   kMagenta),
                    addKnob ("rv_lc",    "LOW CUT", kMagenta), addKnob ("rv_mix",   "MIX",     kMagenta),
                    addKnob ("rv_shim",  "SHIMMER", kMagenta) };

    outKnob = addKnob ("out_gain", "OUTPUT", ui::kViolet);

    ppBtn.setColour     (juce::ToggleButton::tickColourId, kCyan);
    syncBtn.setColour   (juce::ToggleButton::tickColourId, kCyan);
    freezeBtn.setColour (juce::ToggleButton::tickColourId, kMagenta);
    routeBtn.setColour  (juce::ToggleButton::tickColourId, ui::kViolet);
    for (auto* b : { &ppBtn, &syncBtn, &freezeBtn, &routeBtn }) addAndMakeVisible (*b);

    ppAtt   = std::make_unique<BtnAtt> (proc.apvts, "dl_pp",     ppBtn);
    syncAtt = std::make_unique<BtnAtt> (proc.apvts, "dl_sync",   syncBtn);
    frAtt   = std::make_unique<BtnAtt> (proc.apvts, "rv_freeze", freezeBtn);
    rtAtt   = std::make_unique<BtnAtt> (proc.apvts, "serial",    routeBtn);

    // choice parameters -> combo boxes (items must exist before the attachment is made)
    fillChoices (divBox, "dl_div");
    fillChoices (shimBox, "rv_shim_int");
    addAndMakeVisible (divBox);
    addAndMakeVisible (shimBox);
    divAtt  = std::make_unique<BoxAtt> (proc.apvts, "dl_div", divBox);
    shimAtt = std::make_unique<BoxAtt> (proc.apvts, "rv_shim_int", shimBox);

    auto styleCaption = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (ui::makeFont (10.5f, true));
        l.setColour (juce::Label::textColourId, ui::kDim);
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    };
    styleCaption (divCaption, "NOTE VALUE");
    styleCaption (shimCaption, "SHIMMER PITCH");
    styleCaption (infoLabel, {});
    infoLabel.setFont (ui::makeFont (12.0f));

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

AuroraEditor::~AuroraEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuroraEditor::fillChoices (juce::ComboBox& box, const char* paramId)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (c->choices, 1);
}

AuroraEditor::Knob* AuroraEditor::addKnob (const juce::String& paramId, const juce::String& title, juce::Colour accent)
{
    auto* k = knobs.add (new Knob());
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k->slider.setRotaryParameters (1.25f * juce::MathConstants<float>::pi, 2.75f * juce::MathConstants<float>::pi, true);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd9d4f2));
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    k->label.setText (title, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (ui::makeFont (10.5f, true));
    k->label.setColour (juce::Label::textColourId, ui::kDim);
    k->label.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    return k;
}

//==============================================================================
void AuroraEditor::resized()
{
    delayPanel  = { 20.0f,  72.0f, 310.0f, 366.0f };
    reverbPanel = { 670.0f, 72.0f, 310.0f, 366.0f };
    bottomBar   = { 20.0f, 452.0f, 960.0f, 84.0f };
    meterRect   = { 340.0f, 480.0f, 380.0f, 26.0f };

    nebula.setBounds (344, 72, 312, 366);
    presetBox.setBounds (770, 20, 210, 30);

    ppBtn.setBounds     (juce::Rectangle<float> (delayPanel.getRight()  - 134.0f, delayPanel.getY()  + 8.0f, 124.0f, 30.0f).toNearestInt());
    freezeBtn.setBounds (juce::Rectangle<float> (reverbPanel.getRight() - 134.0f, reverbPanel.getY() + 8.0f, 124.0f, 30.0f).toNearestInt());
    routeBtn.setBounds  (40, 476, 262, 34);

    // grid: 4 columns; rows 1-2 = knobs (108 px), row 3 = utility row (92 px)
    auto layoutPanel = [] (juce::Rectangle<float> panel, std::vector<Knob*>& ks)
    {
        const auto grid = panel.withTrimmedTop (50.0f).reduced (6.0f, 0.0f);
        const float cw = grid.getWidth() / 4.0f;
        const float rowY[3] = { grid.getY(), grid.getY() + 108.0f, grid.getY() + 216.0f };
        const float rowH[3] = { 108.0f, 108.0f, 92.0f };
        for (size_t i = 0; i < ks.size(); ++i)
        {
            const size_t row = i / 4, col = i % 4;
            juce::Rectangle<float> cell (grid.getX() + (float) col * cw, rowY[row], cw, rowH[row]);
            ks[i]->label.setBounds (cell.removeFromTop (16.0f).toNearestInt());
            ks[i]->slider.setBounds (cell.toNearestInt());
        }
    };
    layoutPanel (delayPanel, delayKnobs);
    layoutPanel (reverbPanel, reverbKnobs);

    // delay utility row: [ TEMPO SYNC + readout ] [ note value ]
    {
        const auto grid = delayPanel.withTrimmedTop (50.0f).reduced (6.0f, 0.0f);
        const float cw = grid.getWidth() / 4.0f, y3 = grid.getY() + 216.0f;
        syncBtn.setBounds (juce::Rectangle<float> (grid.getX() + 4.0f, y3 + 12.0f, cw * 2.0f - 8.0f, 30.0f).toNearestInt());
        infoLabel.setBounds (juce::Rectangle<float> (grid.getX() + 4.0f, y3 + 46.0f, cw * 2.0f - 8.0f, 36.0f).toNearestInt());
        divCaption.setBounds (juce::Rectangle<float> (grid.getX() + cw * 2.0f, y3 + 8.0f, cw * 2.0f, 16.0f).toNearestInt());
        divBox.setBounds (juce::Rectangle<float> (grid.getX() + cw * 2.0f + 8.0f, y3 + 30.0f, cw * 2.0f - 16.0f, 30.0f).toNearestInt());
    }
    // reverb utility row: [ SHIMMER knob ] [ interval ]
    {
        const auto grid = reverbPanel.withTrimmedTop (50.0f).reduced (6.0f, 0.0f);
        const float cw = grid.getWidth() / 4.0f, y3 = grid.getY() + 216.0f;
        shimCaption.setBounds (juce::Rectangle<float> (grid.getX() + cw, y3 + 8.0f, cw * 3.0f, 16.0f).toNearestInt());
        shimBox.setBounds (juce::Rectangle<float> (grid.getX() + cw + 8.0f, y3 + 30.0f, cw * 3.0f - 16.0f, 30.0f).toNearestInt());
    }

    outKnob->slider.setBounds (850, 454, 100, 80);
    outKnob->label.setJustificationType (juce::Justification::centredRight);
    outKnob->label.setBounds (770, 480, 78, 20);
}

void AuroraEditor::timerCallback()
{
    const juce::String route = routeBtn.getToggleState() ? "SERIAL:  DELAY -> REVERB" : "PARALLEL:  DELAY + REVERB";
    if (routeBtn.getButtonText() != route) routeBtn.setButtonText (route);

    // tempo sync: the free TIME knob is replaced by the note value
    const bool sync = syncBtn.getToggleState();
    delayKnobs[0]->slider.setEnabled (! sync);
    delayKnobs[0]->label.setAlpha (sync ? 0.35f : 1.0f);
    divBox.setEnabled (sync);
    divCaption.setAlpha (sync ? 1.0f : 0.35f);

    const float ms = proc.getEffectiveDelayMs();
    const juce::String msTxt = ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
    const juce::String info = sync ? "= " + msTxt + "\n" + juce::String (juce::roundToInt (proc.getHostBpm())) + " BPM"
                                   : "FREE\n" + msTxt;
    if (infoLabel.getText() != info) infoLabel.setText (info, juce::dontSendNotification);

    repaint (meterRect.expanded (4.0f).toNearestInt());
}

//==============================================================================
void AuroraEditor::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff170b33), 0.0f, 0.0f, juce::Colour (0xff05040b), b.getRight(), b.getBottom(), false));
    g.fillAll();
    drawGlow (g, { b.getWidth() * 0.16f, b.getHeight() * 0.60f }, b.getHeight() * 0.80f, kCyan, 0.10f);
    drawGlow (g, { b.getWidth() * 0.84f, b.getHeight() * 0.60f }, b.getHeight() * 0.80f, kMagenta, 0.11f);

    // title
    g.setGradientFill (juce::ColourGradient (kCyan, 28.0f, 0.0f, kMagenta, 230.0f, 0.0f, false));
    g.setFont (makeFont (36.0f, true));
    g.drawText ("AURORA", juce::Rectangle<float> (26.0f, 6.0f, 260.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (makeFont (11.0f));
    g.drawText ("REVERB  +  DELAY", juce::Rectangle<float> (30.0f, 47.0f, 240.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText ("PRESET", juce::Rectangle<float> (690.0f, 20.0f, 70.0f, 30.0f), juce::Justification::centredRight);

    drawPanel (g, delayPanel,  "DELAY",  "tape echo + host sync",  kCyan);
    drawPanel (g, reverbPanel, "REVERB", "FDN space + shimmer",    kMagenta);

    // bottom bar
    g.setColour (juce::Colour (0x0fffffff));
    g.fillRoundedRectangle (bottomBar, 18.0f);
    g.setColour (juce::Colour (0x1fffffff));
    g.drawRoundedRectangle (bottomBar.reduced (0.5f), 18.0f, 1.0f);

    g.setColour (kDim);
    g.setFont (makeFont (10.5f, true));
    g.drawText ("ROUTING", juce::Rectangle<float> (48.0f, bottomBar.getY() + 6.0f, 120.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText ("OUTPUT LEVEL", juce::Rectangle<float> (meterRect.getX(), bottomBar.getY() + 6.0f, 200.0f, 16.0f), juce::Justification::centredLeft);

    // stereo meter
    auto drawBar = [&] (juce::Rectangle<float> r, float level)
    {
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float n = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (n > 0.01f)
        {
            g.setGradientFill (juce::ColourGradient (kCyan, meterRect.getX(), 0.0f, kMagenta, meterRect.getRight(), 0.0f, false));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * n), r.getHeight() * 0.5f);
        }
    };
    drawBar (meterRect.withHeight (9.0f), proc.getOutLevel (0));
    drawBar (meterRect.withTrimmedTop (16.0f).withHeight (9.0f), proc.getOutLevel (1));
}
