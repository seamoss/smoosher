#include "PluginProcessor.h"

void SmoosherAudioProcessor::parameterChanged (const juce::String&, float)
{
    // No locks, strings, or history allocations on the host's parameter thread.
    ego.invalidate();
    if (localEditThread.load() != juce::Thread::getCurrentThreadId()) ++externalEdits;
}

SmoosherAudioProcessor::Workspace SmoosherAudioProcessor::captureWorkspace() const
{
    const juce::ScopedLock lock (presetLock);
    auto result = workspace;
    result.slots[static_cast<size_t> (result.active)] = { controls(), currentPreset, ego.settings() };
    return result;
}

bool SmoosherAudioProcessor::equal (const Workspace& a, const Workspace& b)
{
    if (a.active != b.active || a.valid != b.valid) return false;
    const auto close = [] (float x, float y) { return std::abs (x - y) < 0.0001f; };
    for (size_t i = 0; i < a.slots.size(); ++i)
    {
        if (! a.valid[i]) continue;
        const auto& x = a.slots[i];
        const auto& y = b.slots[i];
        if (! close (x.controls.smoosh, y.controls.smoosh) || ! close (x.controls.inputDB, y.controls.inputDB)
            || ! close (x.controls.outputDB, y.controls.outputDB) || ! close (x.controls.mix, y.controls.mix)
            || x.preset.name != y.preset.name || ! close (x.preset.smoosh, y.preset.smoosh)
            || ! close (x.preset.inputDB, y.preset.inputDB) || ! close (x.preset.outputDB, y.preset.outputDB)
            || ! close (x.ego.trimDB, y.ego.trimDB) || x.ego.enabled != y.ego.enabled || x.ego.learned != y.ego.learned)
            return false;
    }
    return true;
}

void SmoosherAudioProcessor::recordEdit (const Workspace& before, const Workspace& after, const juce::String& name)
{
    if (equal (before, after)) return;
    if (undoStack.size() >= 64) undoStack.erase (undoStack.begin());
    undoStack.push_back ({ before, after, name });
    redoStack.clear();
}

void SmoosherAudioProcessor::synchroniseHistory()
{
    const juce::ScopedLock lock (presetLock);
    if (editDepth != 0) return;
    if (pendingLearn != 0)
    {
        if (ego.isLearning()) return;
        const auto configuration = ego.word();
        if (ego.completedRequest() == pendingLearn && configuration == ego.completedWord())
        {
            recordEdit (learnBefore, captureWorkspace(), "Ego Check");
            knownConfiguration = configuration;
            updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
        }
        pendingLearn = 0;
    }
    const auto configuration = ego.word();
    if (configuration != knownConfiguration)
    {
        // Automation/state changes supersede local edits. Never undo stale values
        // over a host's newer automation pass.
        undoStack.clear();
        redoStack.clear();
        knownConfiguration = configuration;
    }
}

void SmoosherAudioProcessor::beginUserEdit (const juce::String& name)
{
    const juce::ScopedLock lock (presetLock);
    if (editDepth == 0)
    {
        synchroniseHistory();
        if (pendingLearn != 0) cancelEgo();
        editBefore = captureWorkspace();
        editName = name;
        editExternalRevision = externalEdits.load();
        localEditThread.store (juce::Thread::getCurrentThreadId());
    }
    ++editDepth;
}

void SmoosherAudioProcessor::endUserEdit()
{
    const juce::ScopedLock lock (presetLock);
    if (editDepth == 0 || --editDepth != 0) return;
    localEditThread.store (nullptr);
    if (externalEdits.load() == editExternalRevision)
        recordEdit (editBefore, captureWorkspace(), editName);
    else
    {
        undoStack.clear();
        redoStack.clear();
    }
    knownConfiguration = ego.word();
}

void SmoosherAudioProcessor::applyWorkspace (const Workspace& next)
{
    const auto& snapshot = next.slots[static_cast<size_t> (next.active)];
    const auto set = [this] (const char* id, float value)
    {
        auto* parameter = apvts.getParameter (id);
        const auto normalised = parameter->convertTo0to1 (value);
        if (std::abs (parameter->getValue() - normalised) < 0.000001f) return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    };
    set ("smoosh", snapshot.controls.smoosh);
    set ("inputGain", snapshot.controls.inputDB);
    set ("outputGain", snapshot.controls.outputDB);
    set ("mix", snapshot.controls.mix);
    workspace = next;
    currentPreset = snapshot.preset;
    ego.setSettings (snapshot.ego);
    knownConfiguration = ego.word();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

bool SmoosherAudioProcessor::canUndo()
{
    const juce::ScopedLock lock (presetLock);
    synchroniseHistory();
    return editDepth == 0 && (ego.isLearning() || ! undoStack.empty());
}

bool SmoosherAudioProcessor::canRedo()
{
    const juce::ScopedLock lock (presetLock);
    synchroniseHistory();
    return editDepth == 0 && ! ego.isLearning() && ! redoStack.empty();
}

juce::String SmoosherAudioProcessor::undoName()
{
    const juce::ScopedLock lock (presetLock);
    synchroniseHistory();
    if (editDepth != 0) return {};
    if (ego.isLearning()) return "Cancel learning";
    // The audio thread may finish learning after synchroniseHistory returns.
    return undoStack.empty() ? juce::String() : undoStack.back().name;
}

juce::String SmoosherAudioProcessor::redoName()
{
    const juce::ScopedLock lock (presetLock);
    return canRedo() ? redoStack.back().name : juce::String();
}

void SmoosherAudioProcessor::undo()
{
    const juce::ScopedLock lock (presetLock);
    if (! canUndo()) return;
    if (ego.isLearning()) { cancelEgo(); return; }
    synchroniseHistory();
    if (undoStack.empty()) return;
    const auto edit = undoStack.back();
    undoStack.pop_back();
    applyWorkspace (edit.before);
    redoStack.push_back (edit);
}

void SmoosherAudioProcessor::redo()
{
    const juce::ScopedLock lock (presetLock);
    if (! canRedo()) return;
    const auto edit = redoStack.back();
    redoStack.pop_back();
    applyWorkspace (edit.after);
    undoStack.push_back (edit);
}

int SmoosherAudioProcessor::activeSlot() const
{
    const juce::ScopedLock lock (presetLock);
    return workspace.active;
}

void SmoosherAudioProcessor::selectSlot (int slot)
{
    const juce::ScopedLock lock (presetLock);
    if (slot < 0 || slot > 1 || slot == workspace.active) return;
    beginUserEdit ("Switch A/B");
    auto next = captureWorkspace();
    if (! next.valid[static_cast<size_t> (slot)])
        next.slots[static_cast<size_t> (slot)] = next.slots[static_cast<size_t> (next.active)];
    next.valid[static_cast<size_t> (slot)] = true;
    next.active = slot;
    applyWorkspace (next);
    endUserEdit();
}

void SmoosherAudioProcessor::copyToOtherSlot()
{
    const juce::ScopedLock lock (presetLock);
    beginUserEdit ("Copy A/B");
    workspace = captureWorkspace();
    const auto source = static_cast<size_t> (workspace.active);
    const auto destination = 1 - source;
    workspace.slots[destination] = workspace.slots[source];
    workspace.valid[destination] = true;
    endUserEdit();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void SmoosherAudioProcessor::learnEgo()
{
    const juce::ScopedLock lock (presetLock);
    synchroniseHistory();
    if (editDepth != 0 || ego.isLearning()) return;
    learnBefore = captureWorkspace();
    pendingLearn = ego.learn();
    knownConfiguration = ego.word();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void SmoosherAudioProcessor::cancelEgo()
{
    const juce::ScopedLock lock (presetLock);
    synchroniseHistory();
    if (pendingLearn != 0 || ego.isLearning()) ego.cancel();
    pendingLearn = 0;
    knownConfiguration = ego.word();
}

void SmoosherAudioProcessor::enableEgo (bool enabled)
{
    const juce::ScopedLock lock (presetLock);
    beginUserEdit (enabled ? "Enable Ego Check" : "Disable Ego Check");
    auto settings = ego.settings();
    settings.enabled = enabled && settings.learned;
    ego.setSettings (settings);
    endUserEdit();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void SmoosherAudioProcessor::resetEgo()
{
    const juce::ScopedLock lock (presetLock);
    beginUserEdit ("Reset Ego Check");
    ego.setSettings ({});
    endUserEdit();
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void SmoosherAudioProcessor::writeMoreTrouble (juce::ValueTree& state)
{
    const juce::ScopedLock lock (presetLock);
    state.removeChild (state.getChildWithName ("MoreTrouble"), nullptr);
    juce::ValueTree extra ("MoreTrouble");
    const auto saved = captureWorkspace();
    extra.setProperty ("version", 1, nullptr);
    extra.setProperty ("active", saved.active, nullptr);
    extra.setProperty ("open", drawerOpen.load(), nullptr);
    for (size_t i = 0; i < saved.slots.size(); ++i)
    {
        if (! saved.valid[i]) continue;
        const auto& s = saved.slots[i];
        juce::ValueTree slot ("Slot");
        slot.setProperty ("index", static_cast<int> (i), nullptr);
        slot.setProperty ("smoosh", s.controls.smoosh, nullptr);
        slot.setProperty ("input", s.controls.inputDB, nullptr);
        slot.setProperty ("output", s.controls.outputDB, nullptr);
        slot.setProperty ("mix", s.controls.mix, nullptr);
        slot.setProperty ("presetName", s.preset.name, nullptr);
        slot.setProperty ("presetSmoosh", s.preset.smoosh, nullptr);
        slot.setProperty ("presetInput", s.preset.inputDB, nullptr);
        slot.setProperty ("presetOutput", s.preset.outputDB, nullptr);
        slot.setProperty ("egoTrim", s.ego.trimDB, nullptr);
        slot.setProperty ("egoEnabled", s.ego.enabled, nullptr);
        slot.setProperty ("egoLearned", s.ego.learned, nullptr);
        extra.appendChild (slot, nullptr);
    }
    state.appendChild (extra, nullptr);
}

void SmoosherAudioProcessor::readMoreTrouble (const juce::ValueTree& state)
{
    const juce::ScopedLock lock (presetLock);
    undoStack.clear(); redoStack.clear(); editDepth = 0; pendingLearn = 0;
    localEditThread.store (nullptr);
    workspace = {};
    ego.setSettings ({});
    ego.auditionDry (false);
    drawerOpen.store (false);
    const auto extra = state.getChildWithName ("MoreTrouble");
    if (extra.isValid() && static_cast<int> (extra.getProperty ("version")) == 1)
    {
        for (const auto slot : extra)
        {
            const auto index = static_cast<int> (slot.getProperty ("index", -1));
            if (! slot.hasType ("Slot") || index < 0 || index > 1) continue;
            bool complete = true;
            for (const auto* property : { "smoosh", "input", "output", "mix", "presetName", "presetSmoosh", "presetInput", "presetOutput" })
                complete = complete && slot.hasProperty (property);
            if (! complete) continue;
            Snapshot s;
            s.controls = { static_cast<float> (slot["smoosh"]), static_cast<float> (slot["input"]),
                           static_cast<float> (slot["output"]), static_cast<float> (slot["mix"]) };
            s.preset = { slot["presetName"].toString(), static_cast<float> (slot["presetSmoosh"]),
                         static_cast<float> (slot["presetInput"]), static_cast<float> (slot["presetOutput"]) };
            if (! validPreset (s.preset) || ! validPreset ({ "slot", s.controls.smoosh, s.controls.inputDB, s.controls.outputDB })
                || ! std::isfinite (s.controls.mix) || s.controls.mix < 0 || s.controls.mix > 100) continue;
            s.ego = { static_cast<float> (slot["egoTrim"]), static_cast<bool> (slot["egoEnabled"]), static_cast<bool> (slot["egoLearned"]) };
            if (! std::isfinite (s.ego.trimDB) || s.ego.trimDB < EgoCheck::minimumTrim
                || s.ego.trimDB > EgoCheck::maximumTrim || ! s.ego.learned) s.ego = {};
            workspace.slots[static_cast<size_t> (index)] = s;
            workspace.valid[static_cast<size_t> (index)] = true;
        }
        const auto active = static_cast<int> (extra.getProperty ("active", 0));
        if (active >= 0 && active <= 1 && workspace.valid[static_cast<size_t> (active)]) workspace.active = active;
        ego.setSettings (workspace.slots[static_cast<size_t> (workspace.active)].ego);
        drawerOpen.store (static_cast<bool> (extra["open"]));
    }
    // APVTS and the original preset metadata remain authoritative for the active slot.
    workspace.slots[static_cast<size_t> (workspace.active)] = { controls(), currentPreset, ego.settings() };
    knownConfiguration = ego.word();
}
