#pragma once
#include "PluginEditor.h"

namespace
{
void testDestroyRecall()
{
    SmoosherAudioProcessor p;
    for (size_t i = 0; i < shapeIds.size(); ++i)
        expect (dynamic_cast<juce::AudioProcessorParameterWithID*> (p.getParameters()[static_cast<int> (i)])->paramID == shapeIds[i],
                "The original six host parameter IDs and order must stay unchanged");
    p.loadPreset ({ "Normal A", 48, 6, -4, 75, 90 });
    userSet (p, "mix", 65);
    p.selectSlot (1); p.loadPreset ({ "Normal B", 73, 12, -5.5f, 100, 120 }); p.selectSlot (0);
    p.setMoreTroubleOpen (true); p.setMoreTroublePage (2);
    const auto saved = parameterValues (p);
    p.setDestroyMode (true);
    userSet (p, "destroy", 87);
    expect (parameterValues (p) == saved && p.getCurrentPreset().name == "Normal A", "Destroy must never rewrite Normal's settings or preset identity");
    p.undo(); near (p.destroyAmount(), 0, "Destroy drags must be undoable");
    p.redo(); near (p.destroyAmount(), 87, "Destroy redo must recall its value");
    juce::MemoryBlock state; p.getStateInformation (state);
    SmoosherAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.destroyMode(), "Sessions must restore the Destroy view and processing mode");
    near (restored.destroyAmount(), 87, "Sessions must restore the Destroy knob");
    restored.setDestroyMode (false);
    expect (parameterValues (restored) == saved && restored.getCurrentPreset().name == "Normal A", "Returning must restore all Normal controls and identity");
    expect (restored.moreTroubleOpen() && restored.moreTroublePage() == 2, "Returning must restore Normal's drawer and selected tab");
    restored.selectSlot (1);
    expect (restored.getCurrentPreset().name == "Normal B", "Destroy must preserve the inactive Normal slot");
    near (get (restored, "smoosh"), 73, "Inactive Normal slot settings must survive");

    auto old = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())));
    for (const auto* id : { "destroy", "destroyMode" }) old.removeChild (old.getChildWithProperty ("id", id), nullptr);
    juce::AudioProcessor::copyXmlToBinary (*old.createXml(), state);
    restored.setDestroyMode (true);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (! restored.destroyMode(), "Pre-1.3.1 sessions must explicitly turn Destroy off in an already-used instance");
    near (restored.destroyAmount(), 0, "Old sessions must initialize the Destroy knob at zero");
    expect (parameterValues (restored) == saved, "Old-session migration must retain all six Normal controls");
}

void testDestroyAudio()
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto channels : { 1, 2 })
        {
            SmoosherAudioProcessor p;
            p.loadPreset ({ "Normal", 48, 6, -4, 75, 90 });
            p.prepareToPlay (rate, 17); // Deliberately smaller than incoming blocks.
            SmoosherDSP reference;
            const SmoosherDSP::Controls normal { 48, 6, -4, 100, 75, 90 };
            reference.prepare (rate, normal);
            juce::Random random (131);
            juce::AudioBuffer<float> actual (channels, 257), expected (channels, 257);
            juce::MidiBuffer midi;
            for (int block = 0; block < 100; ++block)
            {
                if (block == 10) { p.setDestroyMode (true); userSet (p, "destroy", 100); }
                if (block == 60) p.setDestroyMode (false);
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < 257; ++i) actual.setSample (ch, i, (random.nextFloat() - 0.5f) * 0.7f);
                expected.makeCopyOf (actual);
                reference.process (expected, normal);
                p.processBlock (actual, midi);
                for (int ch = 0; ch < channels; ++ch)
                {
                    for (int i = 0; i < 257; ++i) expect (std::isfinite (actual.getSample (ch, i)), "Destroy and mode transitions must remain finite");
                    if (block < 10 || block >= 70)
                        expect (std::memcmp (actual.getReadPointer (ch), expected.getReadPointer (ch), 257 * sizeof (float)) == 0,
                                "Normal must remain bit-identical before Destroy and after returning, including its warmed envelopes");
                }
            }
        }

    SmoosherAudioProcessor p;
    p.loadPreset ({ "Matched Normal", 0, 6, 0 });
    p.prepareToPlay (48000, 257); p.learnEgo(); runAudio (p, 48000, 4.5);
    const auto match = p.egoSettings();
    p.setDestroyMode (true); p.prepareToPlay (48000, 257);
    juce::AudioBuffer<float> buffer (2, 4096); juce::MidiBuffer midi;
    fill (buffer, 0.1f); p.processBlock (buffer, midi);
    near (buffer.getSample (0, 4000), 0.1f, "Destroy zero must be clean and independent of Normal's gain and Ego correction");
    expect (p.egoSettings().enabled, "Destroy must preserve Normal's match enable setting");
    near (p.egoSettings().trimDB, match.trimDB, "Destroy must preserve the frozen Normal correction");
    p.setDestroyMode (false);
    fill (buffer, 0.1f); p.processBlock (buffer, midi);
    near (buffer.getSample (0, 4000), 0.1f, "Normal's saved Ego correction must return");
    p.learnEgo(); p.setDestroyMode (true);
    expect (! (p.egoStatus() == EgoCheck::Status::learning), "Entering Destroy must cancel an unfinished Normal measurement");
    p.learnEgo();
    expect (! (p.egoStatus() == EgoCheck::Status::learning), "Destroy cannot start an invisible Ego measurement");

    p.loadPreset ({ "Clipping but hidden", 0, 30, 12 });
    p.prepareToPlay (48000, 17);
    fill (buffer, 0.1f); p.processBlock (buffer, midi);
    expect (! p.consumeMeters().clipped, "An inaudible Normal lane must not trigger the final-output clip meter");

    for (const auto amount : { 0.0f, 15.0f, 30.0f, 50.0f, 75.0f, 100.0f })
    {
        set (p, "destroy", amount); p.prepareToPlay (48000, 17);
        for (int block = 0; block < 20; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4096; ++i)
                    buffer.setSample (ch, i, 0.25f * std::sin (static_cast<float> (block * 4096 + i) * juce::MathConstants<float>::twoPi * 220.0f / 48000.0f));
            p.processBlock (buffer, midi);
            expect (buffer.getMagnitude (0, 4096) < 0.8f, "Fixed compensation must keep the reference signal from becoming a level spike");
        }
    }
}

void testDestroySmoothing()
{
    SmoosherAudioProcessor a, b;
    a.setDestroyMode (true); b.setDestroyMode (true);
    a.prepareToPlay (48000, 2048); b.prepareToPlay (48000, 17);
    set (a, "destroy", 100); set (b, "destroy", 100);
    juce::AudioBuffer<float> whole (2, 2048), part (2, 64); juce::MidiBuffer midi;
    fill (whole, 0.2f); a.processBlock (whole, midi);
    for (int block = 0; block < 32; ++block)
    {
        fill (part, 0.2f); b.processBlock (part, midi);
        for (int i = 0; i < 64; ++i)
            near (whole.getSample (0, block * 64 + i), part.getSample (0, i), "Destroy smoothing must be independent of host and internal chunk sizes");
    }
    SmoosherAudioProcessor transition;
    transition.loadPreset ({ "Normal gain", 0, 6, 0 });
    transition.prepareToPlay (48000, 17);
    transition.setDestroyMode (true);
    fill (whole, 0.1f); transition.processBlock (whole, midi);
    expect (whole.getSample (0, 0) > 0.19f && whole.getSample (0, 959) <= 0.1001f, "Mode changes must crossfade over 20 ms");
    for (int i = 1; i < 2048; ++i)
        expect (std::abs (whole.getSample (0, i) - whole.getSample (0, i - 1)) < 0.001f, "Mode changes must not introduce an abrupt gain step");
}

void testDestroyView()
{
    SmoosherAudioProcessor p;
    p.setMoreTroubleOpen (true); p.setMoreTroublePage (1);
    SmoosherAudioProcessorEditor editor (p);
    juce::TextButton* mode = nullptr;
    for (auto* child : editor.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button && button->getButtonText() == "Destroy") mode = button;
    expect (mode != nullptr, "Normal must offer the Destroy view");
    mode->onClick();
    expect (editor.getWidth() == 420 && editor.getHeight() == 470, "Destroy must use its compact single-knob layout");
    int knobs = 0, buttons = 0;
    for (auto* child : editor.getChildren())
    {
        if (! child->isVisible()) continue;
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
        {
            ++knobs;
            expect (slider->getName() == "Destroy", "Only the Destroy knob may be visible");
            expect (slider->isDoubleClickReturnEnabled(), "Destroy must support a reset to clean");
            juce::Slider::ScopedDragNotification drag (*slider);
            slider->setValue (75, juce::sendNotificationSync);
        }
        if (dynamic_cast<juce::Button*> (child)) ++buttons;
        expect (dynamic_cast<juce::ComboBox*> (child) == nullptr, "Destroy must hide preset and other selectors");
    }
    expect (knobs == 1 && buttons == 1, "Destroy must show just one parameter and the return navigation");
    near (p.destroyAmount(), 75, "The visible knob must control the automatable Destroy amount");
    expect (p.canUndo(), "Destroy gestures must join grouped Undo");
    mode->onClick();
    expect (! p.destroyMode() && editor.getWidth() == 620 && editor.getHeight() == 716, "Return navigation must restore Normal's expanded layout");
}

void testDestroy()
{
    testDestroyRecall(); testDestroyAudio(); testDestroySmoothing(); testDestroyView();
}
}
