#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iterator>

//==============================================================================
// Parameter text helpers
namespace
{
juce::String fmtMs (float v)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; }
juce::String fmtPct (float v)  { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String fmtPctS (float v) { const int r = juce::roundToInt (v); return (r > 0 ? "+" : "") + juce::String (r) + " %"; }
juce::String fmtHz (float v)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
juce::String fmtSec (float v)  { return juce::String (v, 1) + " s"; }
juce::String fmtDb (float v)   { return juce::String (v, 1) + " dB"; }

// note value -> length in quarter-note beats (1/1 ... 1/32, straight / dotted / triplet)
constexpr float kBeats[] = { 4.0f, 2.0f, 3.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f,
                             0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f, 0.125f };
constexpr float kShimSemis[] = { 12.0f, 7.0f, 19.0f, 24.0f, -12.0f };

float parseNumber (const juce::String& t)
{
    float v = t.getFloatValue();
    if (t.containsIgnoreCase ("k")) v *= 1000.0f;
    return v;
}

//==============================================================================
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        { "Init", {} },
        { "Cathedral",
          { { "rv_size", 95.f }, { "rv_decay", 12.f }, { "rv_damp", 4500.f }, { "rv_pre", 45.f }, { "rv_mod", 40.f },
            { "rv_mix", 45.f }, { "dl_mix", 12.f }, { "dl_time", 520.f }, { "dl_fb", 25.f }, { "dl_pp", 0.f } } },
        { "Dub Echo",
          { { "dl_sync", 1.f }, { "dl_fb", 68.f }, { "dl_lc", 200.f }, { "dl_hc", 3200.f }, { "dl_sat", 55.f },
            { "dl_wob", 35.f }, { "dl_mix", 50.f }, { "dl_pp", 0.f }, { "rv_size", 40.f }, { "rv_decay", 1.6f },
            { "rv_mix", 18.f }, { "serial", 1.f } } },
        { "Shimmer Pad",
          { { "rv_size", 85.f }, { "rv_decay", 18.f }, { "rv_damp", 12000.f }, { "rv_mod", 80.f }, { "rv_mix", 55.f },
            { "dl_sync", 1.f }, { "dl_div", 5.f }, { "rv_shim", 55.f }, { "dl_fb", 55.f }, { "dl_hc", 9000.f }, { "dl_wob", 50.f }, { "dl_mix", 30.f },
            { "dl_pp", 1.f }, { "serial", 1.f } } },
        { "Celestial",
          { { "rv_size", 90.f }, { "rv_decay", 10.f }, { "rv_damp", 14000.f }, { "rv_mod", 70.f }, { "rv_mix", 50.f },
            { "rv_shim", 70.f }, { "rv_shim_int", 0.f }, { "dl_sync", 1.f }, { "dl_div", 5.f }, { "dl_fb", 50.f },
            { "dl_mix", 28.f }, { "dl_hc", 8000.f }, { "dl_pp", 1.f }, { "serial", 1.f } } },
        { "Slapback Room",
          { { "dl_time", 110.f }, { "dl_fb", 15.f }, { "dl_mix", 30.f }, { "dl_pp", 0.f }, { "dl_sat", 35.f },
            { "rv_size", 20.f }, { "rv_decay", 0.6f }, { "rv_mix", 15.f }, { "rv_damp", 6000.f } } },
        { "Infinite Space",
          { { "rv_size", 100.f }, { "rv_decay", 30.f }, { "rv_damp", 9000.f }, { "rv_mod", 65.f }, { "rv_mix", 60.f }, { "rv_shim", 35.f },
            { "dl_time", 750.f }, { "dl_fb", 60.f }, { "dl_mix", 25.f }, { "dl_wob", 40.f }, { "serial", 1.f } } },
    };
    return list;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AuroraProcessor::createLayout()
{
    using FmtFn = juce::String (*) (float);
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&] (const char* id, const char* name, float lo, float hi, float skew, float def, FmtFn fmt)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); })
                .withValueFromStringFunction (parseNumber)));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    // Delay
    addF ("dl_time", "Delay Time",     20.0f, 2000.0f, 0.4f, 380.0f, fmtMs);
    addF ("dl_fb",   "Delay Feedback", 0.0f,  95.0f,   1.0f, 45.0f,  fmtPct);
    addF ("dl_lc",   "Delay Low Cut",  20.0f, 1000.0f, 0.4f, 120.0f, fmtHz);
    addF ("dl_hc",   "Delay High Cut", 500.0f, 20000.0f, 0.4f, 6000.0f, fmtHz);
    addF ("dl_sat",  "Delay Saturate", 0.0f,  100.0f,  1.0f, 25.0f,  fmtPct);
    addF ("dl_wob",  "Delay Wobble",   0.0f,  100.0f,  1.0f, 20.0f,  fmtPct);
    addF ("dl_off",  "Delay Offset",   -50.0f, 50.0f,  1.0f, 0.0f,   fmtPctS);
    addF ("dl_mix",  "Delay Mix",      0.0f,  100.0f,  1.0f, 35.0f,  fmtPct);
    addB ("dl_pp",   "Ping-Pong", true);
    addB ("dl_sync", "Delay Tempo Sync", false);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "dl_div", 1 }, "Delay Note Value",
        juce::StringArray { "1/1", "1/2", "1/2 dotted", "1/2 triplet", "1/4", "1/4 dotted", "1/4 triplet",
                            "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32" }, 8));

    // Reverb
    addF ("rv_size",  "Reverb Size",     0.0f,  100.0f, 1.0f, 60.0f,  fmtPct);
    addF ("rv_decay", "Reverb Decay",    0.2f,  30.0f,  0.4f, 4.5f,   fmtSec);
    addF ("rv_damp",  "Reverb Damping",  800.0f, 20000.0f, 0.4f, 7000.0f, fmtHz);
    addF ("rv_pre",   "Reverb Pre-Delay", 0.0f, 200.0f, 0.6f, 20.0f,  fmtMs);
    addF ("rv_mod",   "Reverb Modulation", 0.0f, 100.0f, 1.0f, 35.0f, fmtPct);
    addF ("rv_width", "Reverb Width",    0.0f,  150.0f, 1.0f, 100.0f, fmtPct);
    addF ("rv_lc",    "Reverb Low Cut",  20.0f, 1000.0f, 0.4f, 100.0f, fmtHz);
    addF ("rv_mix",   "Reverb Mix",      0.0f,  100.0f, 1.0f, 30.0f,  fmtPct);
    addB ("rv_freeze", "Reverb Freeze", false);
    addF ("rv_shim",   "Reverb Shimmer", 0.0f, 100.0f, 1.0f, 0.0f, fmtPct);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rv_shim_int", 1 }, "Shimmer Interval",
        juce::StringArray { "Octave up (+12)", "Fifth up (+7)", "Octave + fifth (+19)", "Two octaves (+24)", "Octave down (-12)" }, 0));

    // Master
    addB ("serial", "Serial Routing", false);
    addF ("out_gain", "Output", -24.0f, 6.0f, 1.0f, 0.0f, fmtDb);

    return layout;
}

//==============================================================================
AuroraProcessor::AuroraProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AURORA_STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw.dlTime = get ("dl_time"); raw.dlFb = get ("dl_fb");   raw.dlLc = get ("dl_lc");  raw.dlHc = get ("dl_hc");
    raw.dlSat  = get ("dl_sat");  raw.dlWob = get ("dl_wob"); raw.dlOff = get ("dl_off"); raw.dlMix = get ("dl_mix");
    raw.dlPP   = get ("dl_pp");
    raw.rvSize = get ("rv_size"); raw.rvDecay = get ("rv_decay"); raw.rvDamp = get ("rv_damp"); raw.rvPre = get ("rv_pre");
    raw.rvMod  = get ("rv_mod");  raw.rvWidth = get ("rv_width"); raw.rvLc = get ("rv_lc");    raw.rvMix = get ("rv_mix");
    raw.rvFreeze = get ("rv_freeze");
    raw.dlSync = get ("dl_sync"); raw.dlDiv = get ("dl_div");
    raw.rvShim = get ("rv_shim");  raw.rvShimInt = get ("rv_shim_int");
    raw.serial = get ("serial");  raw.outGain = get ("out_gain");
}

bool AuroraProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == l.getMainInputChannelSet();
}

void AuroraProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    delay.prepare (sr);
    reverb.prepare (sr);
    dlMix.reset ((float) sr, 0.03f);
    rvMix.reset ((float) sr, 0.03f);
    outGain.reset ((float) sr, 0.03f);
    dlMix.setCurrentAndTarget (raw.dlMix->load() * 0.01f);
    rvMix.setCurrentAndTarget (raw.rvMix->load() * 0.01f);
    outGain.setCurrentAndTarget (juce::Decibels::decibelsToGain (raw.outGain->load()));
    rcState[0] = rcState[1] = 0.0f;
    updateParams();
}

void AuroraProcessor::updateParams()
{
    aurora::StereoDelay::Params d;
    float timeMs = raw.dlTime->load();
    if (raw.dlSync->load() > 0.5f)
    {
        const int idx = juce::jlimit (0, (int) std::size (kBeats) - 1, (int) std::lround (raw.dlDiv->load()));
        timeMs = (float) (60000.0 / hostBpm) * kBeats[idx];
        while (timeMs > 2000.0f) timeMs *= 0.5f;   // slow tempi: fold down by octaves to fit the delay line
    }
    effectiveMs.store (timeMs);
    d.timeMs = timeMs;
    d.feedback = raw.dlFb->load() * 0.01f;
    d.lowCutHz = raw.dlLc->load();
    d.highCutHz = raw.dlHc->load();
    d.saturation = raw.dlSat->load() * 0.01f;
    d.wobble = raw.dlWob->load() * 0.01f;
    d.offset = raw.dlOff->load() * 0.01f;
    d.pingPong = raw.dlPP->load() > 0.5f;
    delay.setParams (d);

    aurora::FDNReverb::Params r;
    r.size = raw.rvSize->load() * 0.01f;
    r.decaySec = raw.rvDecay->load();
    r.dampHz = raw.rvDamp->load();
    r.preMs = raw.rvPre->load();
    r.mod = raw.rvMod->load() * 0.01f;
    r.width = raw.rvWidth->load() * 0.01f;
    r.freeze = raw.rvFreeze->load() > 0.5f;
    r.shimmer = raw.rvShim->load() * 0.01f;
    r.shimSemitones = kShimSemis[juce::jlimit (0, (int) std::size (kShimSemis) - 1, (int) std::lround (raw.rvShimInt->load()))];
    reverb.setParams (r);

    rcCoef = 1.0f - std::exp (-aurora::kTwoPi * raw.rvLc->load() / (float) sampleRate);
    dlMix.setTarget (raw.dlMix->load() * 0.01f);
    rvMix.setTarget (raw.rvMix->load() * 0.01f);
    outGain.setTarget (juce::Decibels::decibelsToGain (raw.outGain->load()));
    serial = raw.serial->load() > 0.5f;
}

void AuroraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    if (nCh == 0 || n == 0) return;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    // tempo from the host transport (standalone / hosts without a tempo keep the last value, 120 by default)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm >= 20.0 && *bpm <= 400.0)
                    hostBpm = *bpm;
    bpmShown.store ((float) hostBpm);

    updateParams();

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float peakL = 0.0f, peakR = 0.0f, peakW = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R != nullptr ? R[i] : inL;
        const float dm = dlMix.next(), rm = rvMix.next(), og = outGain.next();

        float tapL, tapR;
        delay.process (inL, inR, tapL, tapR);
        const float dwL = tapL * dm, dwR = tapR * dm;

        // routing: parallel (reverb hears dry) or serial (reverb hears dry + delay)
        float rinL = serial ? inL + dwL : inL;
        float rinR = serial ? inR + dwR : inR;

        // low cut on the reverb input keeps the tail clean
        rcState[0] += rcCoef * (rinL - rcState[0]);
        rcState[1] += rcCoef * (rinR - rcState[1]);
        rinL -= rcState[0];
        rinR -= rcState[1];

        float rvL, rvR;
        reverb.process (rinL, rinR, rvL, rvR);
        rvL *= rm;
        rvR *= rm;

        const float oL = (inL + dwL + rvL) * og;
        const float oR = (inR + dwR + rvR) * og;

        if (R != nullptr) { L[i] = oL; R[i] = oR; }
        else              { L[i] = 0.5f * (oL + oR); }

        peakL = juce::jmax (peakL, std::abs (oL));
        peakR = juce::jmax (peakR, std::abs (oR));
        peakW = juce::jmax (peakW, std::abs (dwL + rvL), std::abs (dwR + rvR));
    }

    const float decay = std::exp (-(float) n / ((float) sampleRate * 0.18f));
    outLevel[0].store (juce::jmax (peakL, outLevel[0].load() * decay));
    outLevel[1].store (juce::jmax (peakR, outLevel[1].load() * decay));
    wetLevel.store (juce::jmax (peakW, wetLevel.load() * decay));
}

//==============================================================================
int AuroraProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String AuroraProcessor::getProgramName (int index)
{
    const auto& list = presets();
    return (index >= 0 && index < (int) list.size()) ? list[(size_t) index].name : juce::String();
}

void AuroraProcessor::setCurrentProgram (int index)
{
    const auto& list = presets();
    if (index < 0 || index >= (int) list.size()) return;
    currentPreset = index;

    for (auto* prm : getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());

    for (const auto& kv : list[(size_t) index].values)
        if (auto* prm = apvts.getParameter (kv.first))
            prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
}

void AuroraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuroraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AuroraProcessor::createEditor() { return new AuroraEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuroraProcessor(); }
