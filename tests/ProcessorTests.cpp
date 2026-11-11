#include "PluginProcessor.h"
#include <iostream>
#include <filesystem>
#include <stdexcept>

namespace
{
void expect (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}
void near (float actual, float expected, const char* message)
{
    expect (std::abs (actual - expected) < 0.0001f, message);
}
void set (SmoosherAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.getAPVTS().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
float get (SmoosherAudioProcessor& p, const char* id) { return p.getAPVTS().getRawParameterValue (id)->load(); }
void fill (juce::AudioBuffer<float>& b, float value)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, value);
}
void testPassthroughAndMeters()
{
    SmoosherAudioProcessor p;
    p.prepareToPlay (48000, 256);
    juce::AudioBuffer<float> b (2, 256);
    juce::MidiBuffer midi;
    fill (b, 0.25f); p.processBlock (b, midi);
    near (b.getSample (0, 100), 0.25f, "Init must be unity");
    auto meters = p.consumeMeters();
    near (meters.left, 0.25f, "Output meter must read final output");
    near (meters.reductionDB, 0.0f, "Init must report no gain reduction");
    near (p.consumeMeters().left, 0.0f, "Meter consumption must clear peaks");
    set (p, "mix", 0); set (p, "inputGain", 30); set (p, "outputGain", 12); set (p, "smoosh", 80);
    p.prepareToPlay (48000, 256);
    fill (b, 0.1f); p.processBlock (b, midi);
    near (b.getSample (0, 100), 0.1f, "Dry mix must ignore wet drive/output");
    near (p.consumeMeters().left, 0.1f, "Meter must follow dry/wet output");
    set (p, "mix", 100); set (p, "smoosh", 0); set (p, "inputGain", 0);
    p.prepareToPlay (48000, 256);
    fill (b, 0.5f); p.processBlock (b, midi);
    meters = p.consumeMeters();
    expect (meters.clipped && meters.left > 1, "Final output clipping must latch");
    fill (b, 0); p.processBlock (b, midi);
    expect (p.consumeMeters().clipped, "Clip indicator must remain latched through silence");
    p.clearClip();
    expect (! p.consumeMeters().clipped, "Clip reset must clear the latch");
}
void testSmoothingAndStereo()
{
    SmoosherDSP dsp;
    dsp.prepare (48000, { 0, 0, 0, 100 });
    juce::AudioBuffer<float> b (2, 1000);
    fill (b, 0.1f); dsp.process (b, { 0, 0, 12, 100 });
    expect (b.getSample (0, 0) < 0.101f, "Gain automation must ramp rather than jump");
    for (int i = 1; i < 960; ++i)
    {
        expect (b.getSample (0, i) >= b.getSample (0, i - 1), "Gain ramp must be monotonic");
        near (b.getSample (0, i), b.getSample (1, i), "Stereo controls must advance together");
    }
    near (b.getSample (0, 999), 0.1f * juce::Decibels::decibelsToGain (12.0f), "Gain must reach target");
    SmoosherDSP whole, split;
    whole.prepare (48000, { 45, 6, 0, 100 }); split.prepare (48000, { 45, 6, 0, 100 });
    juce::AudioBuffer<float> a (2, 1024), chunk (2, 64);
    fill (a, 0.12f); whole.process (a, { 70, 15, -3, 65 });
    for (int block = 0; block < 16; ++block)
    {
        fill (chunk, 0.12f); split.process (chunk, { 70, 15, -3, 65 });
        for (int i = 0; i < 64; ++i)
            near (a.getSample (0, block * 64 + i), chunk.getSample (0, i), "Smoothing must be block-size independent");
    }
    whole.prepare (48000, { 45, 6, 0, 100 });
    fill (a, 0.0f); whole.process (a, { 45, 6, 0, 100 });
    near (a.getMagnitude (0, 1024), 0, "prepareToPlay must clear DSP history");
}
void testReductionAndRates()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        SmoosherDSP dsp;
        dsp.prepare (rate, { 45, 15, 0, 100 });
        juce::AudioBuffer<float> b (2, 512);
        float maximumReduction = 0;
        for (int block = 0; block < 30; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto sample = static_cast<float> (0.5 * std::sin (juce::MathConstants<double>::twoPi * 1000 * (block * 512 + i) / rate));
                b.setSample (0, i, sample); b.setSample (1, i, sample);
            }
            const auto meters = dsp.process (b, { 45, 15, 0, 100 });
            maximumReduction = juce::jmax (maximumReduction, meters.reductionDB);
            for (int i = 0; i < 512; ++i) expect (std::isfinite (b.getSample (0, i)), "Processing must remain finite");
        }
        expect (maximumReduction > 3, "Driven signal must produce compressor gain reduction");
    }
}
void testPresetRecall()
{
    SmoosherAudioProcessor p;
    set (p, "mix", 37);
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[1]);
    near (get (p, "mix"), 37, "Factory presets must preserve Mix");
    expect (! p.isPresetModified(), "Loaded preset must be unmodified");
    set (p, "mix", 50);
    expect (! p.isPresetModified(), "Mix must not mark a preset modified");
    set (p, "inputGain", 16);
    expect (p.isPresetModified(), "Editing preset controls must mark it modified");
    juce::MemoryBlock state;
    p.getStateInformation (state);
    SmoosherAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.getCurrentPreset().name == "Hell Smarsh", "Session recall must preserve preset name");
    expect (restored.isPresetModified(), "Session recall must preserve modified baseline");
    near (get (restored, "inputGain"), 16, "Session recall must preserve edited parameters");
    near (get (restored, "mix"), 50, "Sessions must preserve Mix");
    auto legacy = p.getAPVTS().copyState();
    auto xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.getCurrentPreset().name == "Custom", "Legacy custom settings must not be labelled Init");
    near (get (restored, "inputGain"), 16, "Legacy sessions must retain values");
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[5]);
    near (get (p, "inputGain"), 30, "Dani Time must respect input range");
}
void testUserPresets()
{
    const auto dir = juce::File (std::filesystem::temp_directory_path().u8string())
        .getNonexistentChildFile ("smoosher-test-presets", "", false);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup { dir };
    const auto file = dir.getChildFile ("My preset.smoosherpreset");
    SmoosherAudioProcessor p;
    p.loadPreset ({ "Working", 52, 9, -2 });
    const auto saved = p.saveUserPreset (file, "My preset");
    if (saved.failed())
        throw std::runtime_error (("User preset must save: " + saved.getErrorMessage() + " (" + file.getFullPathName() + ")").toStdString());
    expect (p.saveUserPreset (file, "My preset").failed(), "Saving must not overwrite existing presets");
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[0]); set (p, "mix", 25);
    expect (p.loadUserPreset (file).wasOk(), "User preset must load");
    near (get (p, "smoosh"), 52, "User preset must restore Smoosh");
    near (get (p, "mix"), 25, "User preset must preserve Mix");
    expect (p.getCurrentPreset().name == "My preset" && ! p.isPresetModified(), "Saved preset must retain name");
    file.replaceWithText ("<SmoosherPreset version=\"1\" name=\"Invalid\" smoosh=\"150\" input=\"9\" output=\"-2\"/>");
    expect (p.loadUserPreset (file).failed(), "Out-of-range presets must be rejected");
    near (get (p, "smoosh"), 52, "Invalid preset must not change parameters");
}

void testUnicodePresetRecall()
{
    const auto expectedName = juce::String ("Hard Dick") + juce::String::charToString (0x2122);
    const auto& presets = SmoosherAudioProcessor::factoryPresets();
    const auto preset = std::find_if (presets.begin(), presets.end(), [] (const auto& p)
                                     { return p.name.startsWith ("Hard Dick"); });
    expect (preset != presets.end() && preset->name == expectedName,
            "Factory preset trademark must be one Unicode character, not misdecoded UTF-8 bytes");
    SmoosherAudioProcessor p;
    p.loadPreset (*preset);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    SmoosherAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.getCurrentPreset().name == expectedName,
            "Session recall must preserve the trademark in the preset name");
    expect (! restored.isPresetModified(), "Unicode preset must recall its original settings");
}
}

#include "MoreTroubleTests.h"
#include "ShapeChaosTests.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        testPassthroughAndMeters(); testSmoothingAndStereo(); testReductionAndRates();
        testPresetRecall(); testUserPresets(); testUnicodePresetRecall();
        testMoreTrouble();
        testShapeChaos();
        std::cout << "All processor, metering, automation, and preset checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
