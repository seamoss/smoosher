#pragma once

#include <JuceHeader.h>
#include "SmoosherDSP.h"

//==============================================================================
class SmoosherAudioProcessor  : public juce::AudioProcessor
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

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SmoosherDSP dsp;
    SmoosherDSP::Controls controls() const;
    std::atomic<float>* smooshParameter = nullptr;
    std::atomic<float>* inputParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float> meterLeft { 0 }, meterRight { 0 }, meterReduction { 0 };
    std::atomic<bool> outputClipped { false };
    mutable juce::CriticalSection presetLock;
    Preset currentPreset { "Init", 0, 0, 0 };
    Preset inferPreset() const;
    static bool validPreset (const Preset&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoosherAudioProcessor)
};
