#pragma once
#include <thread>

// Included by ProcessorTests.cpp to share its small assertion/parameter helpers.
namespace
{
void runAudio (SmoosherAudioProcessor& p, double rate, double seconds, float amplitude = 0.1f, int blockSize = 257)
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto total = static_cast<int> (rate * seconds);
    for (int offset = 0; offset < total; offset += blockSize)
    {
        buffer.setSize (2, juce::jmin (blockSize, total - offset), false, false, true);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = amplitude * static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (offset + i) / rate));
            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value * 0.5f);
        }
        p.processBlock (buffer, midi);
    }
}

void userSet (SmoosherAudioProcessor& p, const char* id, float value)
{
    p.beginUserEdit (id); set (p, id, value); p.endUserEdit();
}

void testUndoAndAB()
{
    SmoosherAudioProcessor p;
    expect (! p.canUndo() && ! p.canRedo(), "New instance must have empty history");
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[8]);
    p.undo();
    near (get (p, "smoosh"), 0, "One undo must revert a whole preset load");
    near (get (p, "inputGain"), 0, "Preset undo must include Input");
    expect (p.getCurrentPreset().name == "Init", "Preset undo must restore identity");
    p.redo();
    p.beginUserEdit ("Smoosh drag");
    for (int value = 25; value < 70; ++value) set (p, "smoosh", static_cast<float> (value));
    p.endUserEdit();
    expect (p.isPresetModified(), "Edited preset must show modified state");
    p.undo();
    near (get (p, "smoosh"), 24, "A whole drag must undo in one step");
    expect (! p.isPresetModified(), "Undo must restore the preset baseline");
    p.redo();
    near (get (p, "smoosh"), 69, "Redo must restore the final gesture value");
    userSet (p, "mix", 37);
    p.selectSlot (1);
    near (get (p, "mix"), 37, "First B use must clone A including Mix");
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[16]);
    userSet (p, "mix", 80);
    p.selectSlot (0);
    near (get (p, "smoosh"), 69, "A must retain edited Smoosh");
    near (get (p, "mix"), 37, "A must retain its Mix");
    expect (p.getCurrentPreset().name == "Mr. Cetera" && p.isPresetModified(), "A must retain name and baseline");
    p.copyToOtherSlot();
    p.undo();
    p.selectSlot (1);
    near (get (p, "mix"), 80, "Undo Copy must recover the overwritten B slot");
    expect (p.getCurrentPreset().name == SmoosherAudioProcessor::factoryPresets()[16].name,
            "B must retain Unicode preset identity");
    p.undo();
    expect (p.activeSlot() == 0, "A/B switches must be undoable");
    userSet (p, "outputGain", -9);
    expect (! p.canRedo(), "A new edit must discard the redo branch");
    set (p, "smoosh", 33); // host/external edit, not a local UI gesture
    expect (! p.canUndo() && ! p.canRedo(), "Host automation must invalidate stale local history");
    p.beginUserEdit ("No change"); p.endUserEdit();
    expect (! p.canUndo(), "No-op edits must not create history");
    p.beginUserEdit ("Drag while automation runs");
    set (p, "mix", 20);
    std::thread automation ([&p] { set (p, "outputGain", 7); });
    automation.join();
    p.endUserEdit();
    expect (! p.canUndo(), "Concurrent host automation must invalidate a local gesture");
    for (int i = 0; i < 80; ++i) userSet (p, "mix", static_cast<float> (i));
    int count = 0;
    while (p.canUndo()) { p.undo(); ++count; }
    expect (count == 64, "Local history must have a bounded memory footprint");
}

void testEgoLearning()
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
    {
        SmoosherAudioProcessor p;
        set (p, "inputGain", 6); set (p, "outputGain", 3);
        p.prepareToPlay (rate, 257);
        p.learnEgo();
        runAudio (p, rate, 4.5);
        const auto learned = p.egoSettings();
        expect (learned.learned && learned.enabled, "Learning must finish without an open editor");
        expect (std::abs (learned.trimDB + 9.0f) < 0.002f, "RMS matching must remove a known 9 dB gain at every rate");
        expect (p.undoName() == "Ego Check", "Completed learn must be one undoable edit");
        p.undo();
        expect (! p.egoSettings().learned, "Undo Learn must restore the previous correction");
        p.redo();
        near (p.egoSettings().trimDB, learned.trimDB, "Redo must restore the learned value");
        runAudio (p, rate, 0.25, 0.02f);
        near (p.egoSettings().trimDB, learned.trimDB, "Correction must stay frozen when signal level changes");
        p.consumeMeters();
        juce::AudioBuffer<float> b (2, 2048);
        juce::MidiBuffer midi;
        fill (b, 0.1f); p.processBlock (b, midi);
        near (b.getSample (0, 2047), 0.1f, "Matched output must return to the original input level");
        near (p.consumeMeters().left, 0.1f, "Output meters must include the match correction");
        p.enableEgo (false);
        fill (b, 0.1f); p.processBlock (b, midi);
        expect (b.getSample (0, 0) < 0.101f, "Disabling correction must ramp without a sudden gain jump");
        near (b.getSample (0, 2047), 0.1f * juce::Decibels::decibelsToGain (9.0f), "Match bypass must retain original processing");
        p.auditionDry (true);
        fill (b, 0.1f); p.processBlock (b, midi);
        near (b.getSample (0, 2047), 0.1f, "Dry audition must bypass drive, Mix, and matching");
        p.setMoreTroubleOpen (false);
        fill (b, 0.1f); p.processBlock (b, midi);
        near (b.getSample (0, 2047), 0.1f * juce::Decibels::decibelsToGain (9.0f), "Closing the drawer must release dry audition");
    }
}

void testEgoGatingAndCancellation()
{
    SmoosherAudioProcessor p;
    p.prepareToPlay (48000, 257);
    p.learnEgo();
    runAudio (p, 48000, 5, 0);
    near (p.egoProgress(), 0, "Silence must not count toward learning");
    expect (! p.egoSettings().learned, "Silence must never produce a boost");
    runAudio (p, 48000, 15.1, 0);
    expect (p.egoStatus() == EgoCheck::Status::noSignal, "Insufficient audio must time out after 20 seconds");
    p.learnEgo(); runAudio (p, 48000, 2);
    userSet (p, "inputGain", 6);
    runAudio (p, 48000, 3);
    expect (! p.egoSettings().learned && p.egoStatus() == EgoCheck::Status::cancelled,
            "Editing during learning must cancel it without applying stale gain");
    p.learnEgo(); runAudio (p, 48000, 4.5);
    near (p.egoSettings().trimDB, -6, "Learning must restart cleanly after cancellation");
    p.learnEgo(); runAudio (p, 48000, 1);
    p.cancelEgo();
    near (p.egoSettings().trimDB, -6, "Cancel must retain an earlier correction");
    expect (p.canUndo(), "Cancelling a learn must retain earlier local history");
    p.learnEgo(); set (p, "inputGain", 12);
    expect (p.egoStatus() == EgoCheck::Status::cancelled, "Automation must cancel learning even when transport stops");
    runAudio (p, 48000, 4.5);
    near (p.egoSettings().trimDB, -6, "A cancelled result must never overwrite stored gain");
    expect (! p.canUndo(), "Automation must not leave stale learn history");

    EgoCheck engine;
    engine.prepare (1000);
    engine.learn(); engine.beginBlock();
    for (int i = 0; i < 4500; ++i) engine.measure (0.01, 1.0e-6);
    near (engine.settings().trimDB, EgoCheck::maximumTrim, "Extreme boost must be bounded");
    expect (engine.status() == EgoCheck::Status::limited, "Limited correction must be reported");
}

void testMoreTroubleRecall()
{
    SmoosherAudioProcessor p;
    p.loadPreset ({ "Gain test", 0, 6, 0 });
    p.prepareToPlay (48000, 257);
    p.learnEgo(); runAudio (p, 48000, 4.5);
    p.selectSlot (1);
    p.enableEgo (false);
    p.loadPreset (SmoosherAudioProcessor::factoryPresets()[16]);
    userSet (p, "mix", 42);
    p.setMoreTroubleOpen (true);
    p.auditionDry (true);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    SmoosherAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.activeSlot() == 1 && restored.moreTroubleOpen(), "Session must restore selected slot and drawer");
    near (get (restored, "mix"), 42, "Active slot Mix must round-trip");
    near (restored.egoSettings().trimDB, -6, "Disabled correction must round-trip");
    expect (! restored.egoSettings().enabled && ! restored.canUndo(), "Session loads must clear local history");
    restored.selectSlot (0);
    near (get (restored, "inputGain"), 6, "Inactive slot must round-trip");
    expect (restored.egoSettings().enabled, "Each A/B slot must retain its own match enable state");
    restored.prepareToPlay (48000, 257);
    runAudio (restored, 48000, 0.1);
    near (restored.egoSettings().trimDB, -6, "prepareToPlay must preserve learned correction");
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    auto legacy = juce::ValueTree::fromXml (*xml);
    legacy.removeChild (legacy.getChildWithName ("MoreTrouble"), nullptr);
    juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), state);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.activeSlot() == 0 && ! restored.moreTroubleOpen() && ! restored.egoSettings().learned,
            "Older sessions must reset new features to their original-sound defaults");
    near (get (restored, "mix"), 42, "Legacy session controls must remain intact");
}

void testEgoOffIsUnchanged()
{
    SmoosherDSP original, extended;
    EgoCheck ego;
    const SmoosherDSP::Controls controls { 85, 24, -6, 37 };
    original.prepare (48000, controls); extended.prepare (48000, controls); ego.prepare (48000);
    juce::AudioBuffer<float> a (2, 4096), b (2, 4096);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4096; ++i)
            a.setSample (c, i, 0.3f * std::sin (static_cast<float> (i) * 0.07f + static_cast<float> (c)));
    b.makeCopyOf (a);
    original.process (a, controls); extended.process (b, controls, &ego);
    for (int c = 0; c < 2; ++c)
        expect (std::memcmp (a.getReadPointer (c), b.getReadPointer (c), 4096 * sizeof (float)) == 0,
                "Ego Check off must preserve the existing DSP bit for bit");
}

void testEgoMixAndBlockSizes()
{
    for (const auto blockSize : { 64, 2048 })
    {
        SmoosherAudioProcessor p;
        set (p, "inputGain", 6); set (p, "mix", 25);
        p.prepareToPlay (48000, blockSize);
        p.learnEgo(); runAudio (p, 48000, 4.5, 0.1f, blockSize);
        const auto expected = -juce::Decibels::gainToDecibels (0.75f + 0.25f * juce::Decibels::decibelsToGain (6.0f));
        expect (std::abs (p.egoSettings().trimDB - expected) < 0.002f,
                "Matching must measure the final Mix and be independent of block size");
        p.learnEgo(); runAudio (p, 48000, 1);
        p.selectSlot (1); runAudio (p, 48000, 4);
        near (p.egoSettings().trimDB, expected, "Switching slots must prevent pending measurements from overwriting the destination");
        expect (p.egoStatus() != EgoCheck::Status::learning, "A/B must cancel unfinished learning");
    }
}

void testUndoDuringLearningCompletion()
{
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        SmoosherAudioProcessor p;
        p.prepareToPlay (1000, 32);
        p.learnEgo();
        std::atomic<bool> finished { false };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, 32);
            juce::MidiBuffer midi;
            for (int block = 0; block < 141; ++block)
            {
                fill (buffer, 0.1f);
                p.processBlock (buffer, midi);
            }
            finished.store (true);
        });
        while (! finished.load())
            juce::ignoreUnused (p.undoName());
        audio.join();
        expect (p.canUndo(), "A completed learn must become undoable while history is being polled");
        p.undo();
        expect (! p.egoSettings().learned, "Undo after concurrent completion must restore the previous state");
        p.learnEgo();
        p.undo();
        expect (p.egoStatus() == EgoCheck::Status::cancelled, "Undo with an empty history must cancel an active learn");
    }
}

void testMoreTrouble()
{
    testUndoAndAB(); testEgoLearning(); testEgoGatingAndCancellation(); testMoreTroubleRecall(); testEgoOffIsUnchanged();
    testEgoMixAndBlockSizes();
    testUndoDuringLearningCompletion();
}
}
