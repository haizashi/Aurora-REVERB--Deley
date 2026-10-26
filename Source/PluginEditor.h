#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/AuroraLookAndFeel.h"
#include "UI/NebulaComponent.h"

class AuroraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuroraEditor (AuroraProcessor&);
    ~AuroraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att; // destroyed before the slider
    };

    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using BoxAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    Knob* addKnob (const juce::String& paramId, const juce::String& title, juce::Colour accent);
    void fillChoices (juce::ComboBox& box, const char* paramId);
    void timerCallback() override;

    AuroraProcessor& proc;
    ui::AuroraLookAndFeel laf;
    NebulaComponent nebula;

    juce::OwnedArray<Knob> knobs;
    std::vector<Knob*> delayKnobs, reverbKnobs;
    Knob* outKnob = nullptr;

    juce::ToggleButton ppBtn { "PING-PONG" }, syncBtn { "TEMPO SYNC" }, freezeBtn { "FREEZE" }, routeBtn;
    juce::ComboBox divBox, shimBox, presetBox;
    juce::Label divCaption, shimCaption, infoLabel;
    std::unique_ptr<BtnAtt> ppAtt, syncAtt, frAtt, rtAtt;
    std::unique_ptr<BoxAtt> divAtt, shimAtt;

    juce::Rectangle<float> delayPanel, reverbPanel, bottomBar, meterRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraEditor)
};
