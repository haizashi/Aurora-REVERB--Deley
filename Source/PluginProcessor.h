#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "DSP/FDNReverb.h"
#include "DSP/StereoDelay.h"

class AuroraProcessor : public juce::AudioProcessor
{
public:
    AuroraProcessor();
    ~AuroraProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Aurora"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // for the GUI (thread-safe, written by the audio thread)
    float getOutLevel (int ch) const noexcept { return outLevel[ch & 1].load(); }
    float getWetLevel() const noexcept { return wetLevel.load(); }
    float getEffectiveDelayMs() const noexcept { return effectiveMs.load(); }
    float getHostBpm() const noexcept { return bpmShown.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateParams();

    struct Raw
    {
        std::atomic<float> *dlTime, *dlFb, *dlLc, *dlHc, *dlSat, *dlWob, *dlOff, *dlMix, *dlPP, *dlSync, *dlDiv;
        std::atomic<float> *rvSize, *rvDecay, *rvDamp, *rvPre, *rvMod, *rvWidth, *rvLc, *rvMix, *rvFreeze, *rvShim, *rvShimInt;
        std::atomic<float> *serial, *outGain;
    } raw {};

    aurora::StereoDelay delay;
    aurora::FDNReverb reverb;
    aurora::Ramp dlMix, rvMix, outGain;
    float rcState[2] { 0.0f, 0.0f };
    float rcCoef = 0.01f;
    bool serial = false;
    double sampleRate = 48000.0;
    double hostBpm = 120.0; // last tempo reported by the host (120 when there is none)
    int currentPreset = 0;

    std::atomic<float> outLevel[2] { 0.0f, 0.0f };
    std::atomic<float> wetLevel { 0.0f };
    std::atomic<float> effectiveMs { 380.0f }, bpmShown { 120.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraProcessor)
};
