#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SmoosherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SmoosherLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override;
};

class SmoosherAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmoosherAudioProcessorEditor (SmoosherAudioProcessor&);
    ~SmoosherAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SmoosherAudioProcessor& audioProcessor;
    SmoosherLookAndFeel lookAndFeel;
    juce::Slider smooshSlider, inputGainSlider, outputGainSlider, mixSlider;
    juce::Label smooshLabel, inputGainLabel, outputGainLabel, mixLabel, modifiedLabel;
    juce::ComboBox presetComboBox;
    juce::TextButton saveButton { "Save..." }, clipButton { "PEAK" };
    juce::TextButton moreButton { "More Trouble +" }, aButton { "A" }, bButton { "B" }, copyButton { "Copy A > B" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, learnButton { "Learn" };
    juce::TextButton matchButton { "Match off" }, dryButton { "Hold for Dry" }, resetEgoButton { "Reset" };
    juce::Label egoStatusLabel, egoGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smooshAttachment, inputGainAttachment, outputGainAttachment, mixAttachment;
    juce::Array<juce::File> userPresets;
    juce::TooltipWindow tooltips { this, 600 };
    float displayedLeft = 0, displayedRight = 0, displayedReduction = 0;
    float heldPeak = -100.0f;
    double peakHoldUntil = 0, lastMeterTime = 0;
    bool clipVisible = false;
    bool drawerVisible = false;
    int activeGestures = 0;

    void timerCallback() override;
    void refreshPresetList();
    void updatePresetDisplay();
    void savePreset();
    void configureKnob (juce::Slider&, juce::Label&, const juce::String&, const juce::String&);
    void drawMeters (juce::Graphics&);
    void updateMoreTrouble();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoosherAudioProcessorEditor)
};
