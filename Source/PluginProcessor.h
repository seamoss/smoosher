#pragma once

#include <JuceHeader.h>
#include "SmoosherDSP.h"
#include "DestroyCurve.h"

//==============================================================================
class SmoosherAudioProcessor  : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
public:
    //==============================================================================
    SmoosherAudioProcessor();
    ~SmoosherAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter getters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    struct Preset
    {
        juce::String name;
        float smoosh = 0, inputDB = 0, outputDB = 0;
        float stereoLink = 0, detectorHPF = 0;
    };
    static const std::array<Preset, 17>& factoryPresets();
    void loadPreset (const Preset&);
    Preset getCurrentPreset() const;
    bool isPresetModified() const;
    juce::Result saveUserPreset (const juce::File&, const juce::String& name);
    juce::Result loadUserPreset (const juce::File&);
    static juce::File userPresetDirectory();
    SmoosherDSP::MeterReadings consumeMeters();
    void clearClip() { outputClipped.store (false); }

    // UI edits are grouped explicitly; host automation never enters local Undo.
    void beginUserEdit (const juce::String& name);
    void endUserEdit();
    void synchroniseHistory();
    bool canUndo();
    bool canRedo();
    juce::String undoName();
    juce::String redoName();
    void undo();
    void redo();
    int activeSlot() const;
    void selectSlot (int slot);
    void copyToOtherSlot();
    EgoCheck::Settings egoSettings() const { return ego.settings(); }
    EgoCheck::Status egoStatus() const { return ego.status(); }
    float egoProgress() const { return ego.progress(); }
    void learnEgo();
    void cancelEgo();
    void enableEgo (bool enabled);
    void resetEgo();
    void auditionDry (bool dry) { ego.auditionDry (dry); }
    bool moreTroubleOpen() const { return drawerOpen.load(); }
    void setMoreTroubleOpen (bool open) { drawerOpen.store (open); if (! open) auditionDry (false); }
    int moreTroublePage() const { return drawerPage.load(); }
    void setMoreTroublePage (int page);
    int chaosAmount() const { return variationAmount.load(); }
    void setChaosAmount (int amount);
    // Lock order: Smoosh, Input, Output, Mix, Stereo Link, Detector HPF.
    bool chaosLocked (int index) const;
    void setChaosLocked (int index, bool locked);
    bool canGenerateVariation() const { return variationLocks.load() != 63; }
    bool generateVariation();
    bool destroyMode() const { return destroyModeParameter->load() >= 0.5f; }
    float destroyAmount() const { return destroyParameter->load(); }
    void setDestroyMode (bool);

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SmoosherDSP dsp;
    SmoosherDSP destroyDSP;
    juce::AudioBuffer<float> destroyBuffer;
    juce::SmoothedValue<float> destroyBlend;
    EgoCheck ego;
    SmoosherDSP::Controls controls() const;
    SmoosherDSP::MeterReadings processModes (juce::AudioBuffer<float>&);
    std::atomic<float>* smooshParameter = nullptr;
    std::atomic<float>* inputParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* linkParameter = nullptr;
    std::atomic<float>* detectorParameter = nullptr;
    std::atomic<float>* destroyParameter = nullptr;
    std::atomic<float>* destroyModeParameter = nullptr;
    std::atomic<float> meterLeft { 0 }, meterRight { 0 }, meterReduction { 0 };
    std::atomic<bool> outputClipped { false };
    mutable juce::CriticalSection presetLock;
    Preset currentPreset { "Init", 0, 0, 0 };
    Preset inferPreset() const;
    static bool validPreset (const Preset&);
    static bool validShape (float link, float cutoff);

    struct Snapshot
    {
        SmoosherDSP::Controls controls { 0, 0, 0, 100 };
        Preset preset { "Init", 0, 0, 0 };
        EgoCheck::Settings ego;
    };
    struct Workspace
    {
        std::array<Snapshot, 2> slots;
        std::array<bool, 2> valid {{ true, false }};
        int active = 0;
        float destroy = 0;
        bool destroyMode = false;
    };
    struct Edit { Workspace before, after; juce::String name; };
    Workspace workspace, editBefore, learnBefore;
    std::vector<Edit> undoStack, redoStack;
    juce::String editName;
    int editDepth = 0;
    uint64_t knownConfiguration = 0, pendingLearn = 0;
    std::atomic<juce::Thread::ThreadID> localEditThread { nullptr };
    std::atomic<uint64_t> externalEdits { 0 };
    uint64_t editExternalRevision = 0;
    std::atomic<bool> drawerOpen { false };
    std::atomic<int> drawerPage { 0 }, variationAmount { 0 }, variationLocks { 12 };
    juce::Random variationRandom;
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override { synchroniseHistory(); }
    Workspace captureWorkspace() const;
    void applyWorkspace (const Workspace&);
    void recordEdit (const Workspace&, const Workspace&, const juce::String&);
    static bool equal (const Workspace&, const Workspace&);
    void writeMoreTrouble (juce::ValueTree&);
    void readMoreTrouble (const juce::ValueTree&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoosherAudioProcessor)
};
