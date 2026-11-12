#pragma once
#include "LegacyDSP12.h"

namespace
{
constexpr std::array<const char*, 6> shapeIds {{ "smoosh", "inputGain", "outputGain", "mix", "stereoLink", "detectorHPF" }};
std::array<float, 6> parameterValues (SmoosherAudioProcessor& p)
{
    std::array<float, 6> values;
    for (size_t i = 0; i < values.size(); ++i) values[i] = get (p, shapeIds[i]);
    return values;
}

void testFrozen12Sound()
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto channels : { 1, 2 })
        {
            LegacyDSP12 legacy;
            SmoosherDSP current;
            legacy.prepare (rate, { 0, 0, 0, 100 });
            current.prepare (rate, { 0, 0, 0, 100 });
            juce::Random random (812);
            juce::AudioBuffer<float> a (channels, 257), b (channels, 257);
            const std::array<SmoosherDSP::Controls, 5> settings {{
                { 0, 0, 0, 100 }, { 48, 6, -4, 100 }, { 90, 24, -9, 37 },
                { 30, 30, 2, 0 }, { 0, 0, 0, 100 }
            }};
            for (const auto c : settings)
                for (int block = 0; block < 20; ++block)
                {
                    for (int ch = 0; ch < channels; ++ch)
                        for (int i = 0; i < 257; ++i) a.setSample (ch, i, random.nextFloat() * 1.4f - 0.7f);
                    b.makeCopyOf (a);
                    const auto oldMeters = legacy.process (a, { c.smoosh, c.inputDB, c.outputDB, c.mix });
                    const auto newMeters = current.process (b, c);
                    for (int ch = 0; ch < channels; ++ch)
                        expect (std::memcmp (a.getReadPointer (ch), b.getReadPointer (ch), 257 * sizeof (float)) == 0,
                                "Default 1.3 processing must match the frozen 1.2 DSP bit for bit, including transitions");
                    near (newMeters.reductionDB, oldMeters.reductionDB, "Legacy gain-reduction meters must remain unchanged");
                }
        }
}

void testStereoLinkAndDetector()
{
    std::array<double, 3> rightEnergy {}, leftEnergy {};
    for (int setting = 0; setting < 3; ++setting)
    {
        SmoosherDSP dsp;
        const SmoosherDSP::Controls c { 50, 12, -6, 100, static_cast<float> (setting * 50), 0 };
        dsp.prepare (48000, c);
        juce::AudioBuffer<float> b (2, 512);
        for (int block = 0; block < 100; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto wave = static_cast<float> (std::sin ((block * 512 + i) * juce::MathConstants<double>::twoPi * 1000 / 48000));
                b.setSample (0, i, wave * 0.7f); b.setSample (1, i, wave * 0.04f);
            }
            dsp.process (b, c);
            if (block >= 80)
                for (int i = 0; i < 512; ++i)
                {
                    leftEnergy[static_cast<size_t> (setting)] += std::pow (b.getSample (0, i), 2);
                    rightEnergy[static_cast<size_t> (setting)] += std::pow (b.getSample (1, i), 2);
                }
        }
    }
    expect (rightEnergy[2] < rightEnergy[1] && rightEnergy[1] < rightEnergy[0],
            "Increasing link must progressively share the stronger channel's reduction");
    expect (std::abs (leftEnergy[2] - leftEnergy[0]) < 1.0e-6,
            "Linking must retain the stronger channel's existing reduction");

    const auto reduction = [] (float cutoff, double frequency)
    {
        SmoosherDSP dsp;
        const SmoosherDSP::Controls c { 40, 15, -6, 100, 0, cutoff };
        dsp.prepare (48000, c);
        juce::AudioBuffer<float> b (1, 512);
        float last = 0;
        for (int block = 0; block < 100; ++block)
        {
            for (int i = 0; i < 512; ++i)
                b.setSample (0, i, 0.5f * static_cast<float> (std::sin ((block * 512 + i) * juce::MathConstants<double>::twoPi * frequency / 48000)));
            last = dsp.process (b, c).reductionDB;
        }
        return last;
    };
    expect (reduction (200, 45) + 3 < reduction (0, 45), "Kick Privilege must reduce low-frequency-triggered compression");
    expect (reduction (200, 1000) > 1, "Detector filtering must still respond to higher-frequency energy");

    SmoosherDSP off, on;
    off.prepare (48000, { 45, 0, 0, 100, 0, 0 });
    on.prepare (48000, { 45, 0, 0, 100, 100, 300 });
    juce::AudioBuffer<float> a (1, 4096), b (1, 4096);
    for (int i = 0; i < 4096; ++i) a.setSample (0, i, 0.0001f * std::sin (static_cast<float> (i) * 0.01f));
    b.makeCopyOf (a);
    off.process (a, { 45, 0, 0, 100, 0, 0 }); on.process (b, { 45, 0, 0, 100, 100, 300 });
    expect (std::memcmp (a.getReadPointer (0), b.getReadPointer (0), 4096 * sizeof (float)) == 0,
            "The bass filter must not EQ audio below threshold, and stereo link must not affect mono");

    SmoosherDSP whole, split;
    whole.prepare (48000, { 50, 12, -6, 100 }); split.prepare (48000, { 50, 12, -6, 100 });
    juce::AudioBuffer<float> full (2, 2048), chunk (2, 64);
    fill (full, 0.4f); whole.process (full, { 50, 12, -6, 100, 100, 200 });
    for (int block = 0; block < 32; ++block)
    {
        fill (chunk, 0.4f); split.process (chunk, { 50, 12, -6, 100, 100, 200 });
        for (int i = 0; i < 64; ++i)
        {
            expect (std::isfinite (chunk.getSample (0, i)), "Shape automation must stay finite");
            near (full.getSample (0, block * 64 + i), chunk.getSample (0, i), "Shape smoothing must be block-size independent");
        }
    }
}

void testShapeRecallAndPresets()
{
    SmoosherAudioProcessor p;
    const std::array<juce::String, 4> originalNames {{ "Smoosh", "Input", "Output", "Mix" }};
    expect (p.getParameters().size() == 8, "Shape and Destroy parameters must follow the original parameters");
    for (int i = 0; i < 4; ++i) expect (p.getParameters()[i]->getName (32) == originalNames[static_cast<size_t> (i)], "Original parameter order must remain stable");
    p.loadPreset ({ "Shape A", 48, 6, -4, 75, 90 });
    p.selectSlot (1);
    userSet (p, "stereoLink", 100); userSet (p, "detectorHPF", 180);
    expect (p.isPresetModified(), "Shape edits must modify the preset indicator");
    p.undo(); near (get (p, "detectorHPF"), 90, "Undo must restore detector settings");
    p.redo();
    p.setMoreTroubleOpen (true); p.setMoreTroublePage (2); p.setChaosAmount (2); p.setChaosLocked (0, true);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    SmoosherAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    near (get (restored, "stereoLink"), 100, "Active slot link must round-trip");
    near (get (restored, "detectorHPF"), 180, "Active slot filter must round-trip");
    expect (restored.isPresetModified(), "Shape preset baseline must round-trip");
    expect (restored.moreTroublePage() == 2 && restored.chaosAmount() == 2 && restored.chaosLocked (0), "Tab, amount, and locks must round-trip");
    restored.selectSlot (0);
    near (get (restored, "stereoLink"), 75, "Inactive slot link must round-trip");
    near (get (restored, "detectorHPF"), 90, "Inactive slot filter must round-trip");
    expect (! restored.isPresetModified(), "Unmodified shape presets must recall correctly");

    auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())));
    for (const auto* id : { "stereoLink", "detectorHPF" }) tree.removeChild (tree.getChildWithProperty ("id", id), nullptr);
    tree.removeProperty ("presetLink", nullptr); tree.removeProperty ("presetHPF", nullptr);
    auto extra = tree.getChildWithName ("MoreTrouble");
    extra.setProperty ("version", 1, nullptr);
    for (auto slot : extra)
        for (const auto* property : { "stereoLink", "detectorHPF", "presetLink", "presetHPF" }) slot.removeProperty (property, nullptr);
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), state);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    near (get (restored, "stereoLink"), 0, "A 1.2 session must reset already-used link controls");
    near (get (restored, "detectorHPF"), 0, "A 1.2 session must reset already-used detector controls");
    restored.selectSlot (0);
    near (get (restored, "stereoLink"), 0, "Legacy inactive slots must use independent stereo");
    near (get (restored, "detectorHPF"), 0, "Legacy inactive slots must bypass the new filter");
    expect (restored.moreTroublePage() == 0 && restored.chaosAmount() == 0 && ! restored.chaosLocked (0), "Legacy sessions must default new UI preferences");

    const auto dir = juce::File (std::filesystem::temp_directory_path().string()).getChildFile ("smoosher-shape-" + juce::Uuid().toString());
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup { dir };
    const auto file = dir.getChildFile ("Shape.smoosherpreset");
    p.selectSlot (0);
    expect (p.saveUserPreset (file, "Shape preset").wasOk(), "Version 2 preset must save");
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[0]);
    expect (p.loadUserPreset (file).wasOk(), "Version 2 preset must load");
    near (get (p, "stereoLink"), 75, "User preset must preserve link");
    near (get (p, "detectorHPF"), 90, "User preset must preserve filter");
    file.replaceWithText ("<SmoosherPreset version=\"1\" name=\"Old preset\" smoosh=\"48\" input=\"6\" output=\"-4\"/>");
    expect (p.loadUserPreset (file).wasOk(), "Version 1 preset files must remain supported");
    near (get (p, "stereoLink"), 0, "Old presets must reset link");
    near (get (p, "detectorHPF"), 0, "Old presets must reset filtering");
    file.replaceWithText ("<SmoosherPreset version=\"2\" name=\"Invalid\" smoosh=\"48\" input=\"6\" output=\"-4\" stereoLink=\"50\" detectorHPF=\"7\"/>");
    expect (p.loadUserPreset (file).failed(), "Invalid shape ranges must be rejected");
}

void testBadDecisions()
{
    SmoosherAudioProcessor p;
    expect (p.chaosLocked (2) && p.chaosLocked (3), "Output and Mix must start locked");
    p.loadPreset ({ "Reference A", 48, 6, -4, 75, 90 });
    userSet (p, "mix", 65);
    p.prepareToPlay (48000, 257);
    p.learnEgo(); runAudio (p, 48000, 4.5);
    const auto match = p.egoSettings();
    expect (match.learned && match.enabled, "Randomization fixture must have an active learned match");
    p.selectSlot (1);
    p.loadPreset ({ "Original B", 20, 3, -8, 10, 220 });
    p.selectSlot (0);
    const auto before = parameterValues (p);
    expect (p.generateVariation(), "Bad Decisions must generate a variation");
    expect (parameterValues (p) == before && p.activeSlot() == 0 && p.getCurrentPreset().name == "Reference A",
            "Generating must not alter any active control or identity");
    near (p.egoSettings().trimDB, match.trimDB, "Generating must preserve the active slot's match");
    expect (p.egoSettings().enabled, "Generating must not bypass the active slot's match");
    expect (p.undoName() == "Bad Decisions", "Generation must be one undo step");
    p.selectSlot (1);
    near (get (p, "outputGain"), before[2], "Output lock must copy the source value");
    near (get (p, "mix"), before[3], "Mix lock must copy the source value");
    expect (! p.egoSettings().learned, "Generated variants must not inherit stale matching");
    p.undo(); p.undo(); // switch, then generation
    p.selectSlot (1);
    expect (p.getCurrentPreset().name == "Original B", "Undo generation must recover the replaced slot");
    near (get (p, "detectorHPF"), 220, "Undo generation must restore the old slot's shape");
    for (int i = 0; i < 6; ++i) p.setChaosLocked (i, true);
    const auto unchanged = parameterValues (p);
    expect (! p.generateVariation() && parameterValues (p) == unchanged, "All locks must make generation a no-op");

    for (int amount = 0; amount < 3; ++amount)
        for (int unlocked = 0; unlocked < 6; ++unlocked)
        {
            p.setChaosAmount (amount);
            for (int i = 0; i < 6; ++i) p.setChaosLocked (i, i != unlocked);
            for (int roll = 0; roll < 12; ++roll)
            {
                const auto original = parameterValues (p);
                expect (p.generateVariation(), "An unlocked control must produce a variation");
                expect (parameterValues (p) == original, "Every roll must leave the active sound untouched");
                p.selectSlot (1 - p.activeSlot());
                const auto generated = parameterValues (p);
                for (size_t i = 0; i < generated.size(); ++i)
                {
                    if (static_cast<int> (i) != unlocked) near (generated[i], original[i], "Every parameter lock must be honored");
                    expect (std::isfinite (generated[i]), "Generated values must be finite");
                }
                expect (std::abs (generated[static_cast<size_t> (unlocked)] - original[static_cast<size_t> (unlocked)]) > 0.001f,
                        "Even boundary rolls must change an unlocked parameter");
                expect (generated[0] >= 0 && generated[0] <= 100 && generated[1] >= 0 && generated[1] <= 30
                        && generated[2] >= -12 && generated[2] <= 12 && generated[3] >= 0 && generated[3] <= 100
                        && generated[4] >= 0 && generated[4] <= 100 && generated[5] >= 0 && generated[5] <= 300
                        && (generated[5] < 0.1f || generated[5] >= 20), "Every variation must stay inside legal ranges");
            }
        }
}

void testLegacyMatchedSession()
{
    SmoosherAudioProcessor p;
    p.loadPreset ({ "Old gain", 0, 6, 0 });
    p.prepareToPlay (48000, 257);
    p.learnEgo(); runAudio (p, 48000, 4.5);
    p.selectSlot (1); p.enableEgo (false); p.selectSlot (0);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())));
    for (const auto* id : { "stereoLink", "detectorHPF" }) tree.removeChild (tree.getChildWithProperty ("id", id), nullptr);
    tree.removeProperty ("presetLink", nullptr); tree.removeProperty ("presetHPF", nullptr);
    auto extra = tree.getChildWithName ("MoreTrouble");
    extra.setProperty ("version", 1, nullptr);
    for (auto slot : extra)
        for (const auto* property : { "stereoLink", "detectorHPF", "presetLink", "presetHPF" }) slot.removeProperty (property, nullptr);
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), state);
    set (p, "stereoLink", 100); set (p, "detectorHPF", 200);
    p.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    near (p.egoSettings().trimDB, -6, "Legacy learned gain must survive 1.3 migration");
    expect (p.egoSettings().enabled, "Legacy active match must remain enabled");
    p.prepareToPlay (48000, 1024);
    juce::AudioBuffer<float> buffer (2, 1024); juce::MidiBuffer midi;
    fill (buffer, 0.1f); p.processBlock (buffer, midi);
    near (buffer.getSample (0, 1023), 0.1f, "A migrated matched session must sound the same");
    p.selectSlot (1);
    expect (! p.egoSettings().enabled && p.egoSettings().learned, "Legacy inactive slot match settings must survive");
    p.learnEgo(); userSet (p, "stereoLink", 50);
    expect (p.egoStatus() == EgoCheck::Status::cancelled, "Link automation must invalidate pending matching");
    p.learnEgo(); userSet (p, "detectorHPF", 100);
    expect (p.egoStatus() == EgoCheck::Status::cancelled, "Detector automation must invalidate pending matching");
}

void testShapeChaos()
{
    testFrozen12Sound(); testStereoLinkAndDetector(); testShapeRecallAndPresets(); testBadDecisions();
    testLegacyMatchedSession();
}
}
