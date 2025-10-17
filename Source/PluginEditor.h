#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom LookAndFeel for gradient sliders (green to red)
class GradientSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
};

// Custom LookAndFeel for smaller ComboBox font
class SmallComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font(11.0f);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(11.0f);
    }
};

//==============================================================================
class SmoosherAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SmoosherAudioProcessorEditor (SmoosherAudioProcessor&);
    ~SmoosherAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SmoosherAudioProcessor& audioProcessor;

    // Sliders
    juce::Slider smooshSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::Slider mixSlider;

    // Labels
    juce::Label smooshLabel;
    juce::Label inputGainLabel;
    juce::Label outputGainLabel;
    juce::Label mixLabel;

    // Preset selector
    juce::ComboBox presetComboBox;
    juce::Label presetLabel;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smooshAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // Custom LookAndFeel
    GradientSliderLookAndFeel gradientLookAndFeel;
    SmallComboBoxLookAndFeel comboBoxLookAndFeel;

    // Preset management
    struct Preset
    {
        juce::String name;
        float smoosh;
        float inputGain;
        float outputGain;
    };

    std::vector<Preset> factoryPresets;
    void loadPreset(int presetIndex);
    void setupPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoosherAudioProcessorEditor)
};
