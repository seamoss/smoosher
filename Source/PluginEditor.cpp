#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Custom LookAndFeel Implementation
void GradientSliderLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                   float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                                   juce::Slider& slider)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
    auto centreX = (float) x + (float) width * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Interpolate color from green (0) to red (1) based on slider position
    juce::Colour fillColour = juce::Colour::fromHSV(
        juce::jmap(sliderPos, 0.33f, 0.0f),  // Hue: 0.33 (green) to 0.0 (red)
        0.8f,  // Saturation
        0.7f,  // Brightness
        1.0f   // Alpha
    );

    // Draw the filled arc
    g.setColour(fillColour);
    juce::Path filledArc;
    filledArc.addPieSegment(rx, ry, rw, rw, rotaryStartAngle, angle, 0.8f);
    g.fillPath(filledArc);

    // Draw the background arc (unfilled portion)
    g.setColour(juce::Colour(0xff404040));
    juce::Path backgroundArc;
    backgroundArc.addPieSegment(rx, ry, rw, rw, angle, rotaryEndAngle, 0.8f);
    g.fillPath(backgroundArc);

    // Draw the outline
    g.setColour(juce::Colour(0xff606060));
    g.drawEllipse(rx, ry, rw, rw, 2.0f);

    // Draw the pointer
    juce::Path pointer;
    auto pointerLength = radius * 0.6f;
    auto pointerThickness = 3.0f;
    pointer.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
    g.setColour(juce::Colours::white);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
}

//==============================================================================
SmoosherAudioProcessorEditor::SmoosherAudioProcessorEditor (SmoosherAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set window size (with room for preset selector and mix control)
    setSize (500, 260);

    // Setup factory presets
    setupPresets();

    // Configure preset selector
    presetLabel.setText("PRESET:", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(presetLabel);

    presetComboBox.addItem("-- Init --", 1);
    for (int i = 0; i < factoryPresets.size(); ++i)
        presetComboBox.addItem(factoryPresets[i].name, i + 2);

    presetComboBox.setSelectedId(1, juce::dontSendNotification);
    presetComboBox.onChange = [this] {
        int selectedId = presetComboBox.getSelectedId();
        if (selectedId == 1)
        {
            // Load Init preset (all zeros)
            audioProcessor.getAPVTS().getParameter("smoosh")->setValueNotifyingHost(0.0f);
            audioProcessor.getAPVTS().getParameter("inputGain")->setValueNotifyingHost(0.0f);
            audioProcessor.getAPVTS().getParameter("outputGain")->setValueNotifyingHost(0.5f); // 0.5 = 0dB (middle of -12 to +12)
        }
        else if (selectedId > 1)
        {
            loadPreset(selectedId - 2);
        }
    };

    // Set custom look and feel for smaller font
    presetComboBox.setLookAndFeel(&comboBoxLookAndFeel);
    presetComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);

    addAndMakeVisible(presetComboBox);

    // Configure Smoosh slider (main knob - center)
    smooshSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    smooshSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    smooshSlider.setScrollWheelEnabled(true);
    smooshSlider.setLookAndFeel(&gradientLookAndFeel);
    addAndMakeVisible(smooshSlider);

    smooshLabel.setText("SMOOSH", juce::dontSendNotification);
    smooshLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    smooshLabel.setJustificationType(juce::Justification::centred);
    smooshLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    smooshLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(smooshLabel);

    smooshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "smoosh", smooshSlider);

    // Configure Input Gain slider
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setScrollWheelEnabled(true);
    inputGainSlider.setLookAndFeel(&gradientLookAndFeel);
    addAndMakeVisible(inputGainSlider);

    inputGainLabel.setText("INPUT", juce::dontSendNotification);
    inputGainLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    inputGainLabel.setJustificationType(juce::Justification::centred);
    inputGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    inputGainLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(inputGainLabel);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "inputGain", inputGainSlider);

    // Configure Output Gain slider
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    outputGainSlider.setScrollWheelEnabled(true);
    outputGainSlider.setLookAndFeel(&gradientLookAndFeel);
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("GAIN", juce::dontSendNotification);
    outputGainLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    outputGainLabel.setJustificationType(juce::Justification::centred);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputGainLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(outputGainLabel);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "outputGain", outputGainSlider);

    // Configure Mix slider
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setScrollWheelEnabled(true);
    mixSlider.setLookAndFeel(&gradientLookAndFeel);
    addAndMakeVisible(mixSlider);

    mixLabel.setText("MIX", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    mixLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    mixLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(mixLabel);

    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "mix", mixSlider);
}

SmoosherAudioProcessorEditor::~SmoosherAudioProcessorEditor()
{
    // Clean up LookAndFeel
    smooshSlider.setLookAndFeel(nullptr);
    inputGainSlider.setLookAndFeel(nullptr);
    outputGainSlider.setLookAndFeel(nullptr);
    mixSlider.setLookAndFeel(nullptr);
    presetComboBox.setLookAndFeel(nullptr);
}

//==============================================================================
void SmoosherAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void SmoosherAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Preset selector at top right
    auto presetArea = bounds.removeFromTop(30);
    presetArea.removeFromTop(5); // Small top margin

    // Align to the right
    auto presetWidth = 250;
    presetArea.removeFromLeft(bounds.getWidth() - presetWidth); // Push to right
    presetComboBox.setBounds(presetArea.removeFromRight(presetWidth - 65).reduced(5, 0));
    presetLabel.setBounds(presetArea.reduced(0, 0));

    bounds.removeFromTop(-5); // Reduced spacing to close gap

    // Divide into three equal columns for controls
    int controlWidth = bounds.getWidth() / 3;

    // Input (left)
    auto inputArea = bounds.removeFromLeft(controlWidth);
    auto inputSliderBounds = inputArea.withSizeKeepingCentre(110, 110);
    inputGainSlider.setBounds(inputSliderBounds);
    inputGainLabel.setBounds(inputSliderBounds);

    // Smoosh (center - slightly larger)
    auto smooshArea = bounds.removeFromLeft(controlWidth);
    auto smooshSliderBounds = smooshArea.withSizeKeepingCentre(140, 140);
    smooshSlider.setBounds(smooshSliderBounds);
    smooshLabel.setBounds(smooshSliderBounds);

    // Output (right)
    auto outputArea = bounds;
    auto outputSliderBounds = outputArea.withSizeKeepingCentre(110, 110);
    outputGainSlider.setBounds(outputSliderBounds);
    outputGainLabel.setBounds(outputSliderBounds);

    // Mix control at bottom right (below main controls)
    auto mixArea = getLocalBounds().removeFromBottom(64).removeFromRight(69).reduced(5);
    auto mixSliderBounds = mixArea.withSizeKeepingCentre(48, 48);
    mixSlider.setBounds(mixSliderBounds);
    mixLabel.setBounds(mixSliderBounds);
}

//==============================================================================
void SmoosherAudioProcessorEditor::setupPresets()
{
    // Factory presets
    // Format: {"Name", Smoosh %, Input dB (0-30), Output/Gain dB (-12 to +12)}
    factoryPresets.push_back({"Hell Smarsh", 45.0f, 15.0f, 1.0f});
    factoryPresets.push_back({"Wenger's 7-String", 60.0f, 10.5f, 2.0f});
    factoryPresets.push_back({"That Joe Sound", 70.0f, 15.0f, 0.0f});
    factoryPresets.push_back({"Gravy River", 70.0f, 12.0f, 2.0f});
    factoryPresets.push_back({"Dani Time", 30.0f, 35.0f, 2.0f});
    factoryPresets.push_back({"Gloopy", 40.0f, 7.5f, 1.0f});
    factoryPresets.push_back({"Limit THIS!", 75.0f, 24.0f, 3.0f});
}

void SmoosherAudioProcessorEditor::loadPreset(int presetIndex)
{
    if (presetIndex >= 0 && presetIndex < factoryPresets.size())
    {
        const auto& preset = factoryPresets[presetIndex];

        // Update parameter values
        auto* smooshParam = audioProcessor.getAPVTS().getRawParameterValue("smoosh");
        auto* inputParam = audioProcessor.getAPVTS().getRawParameterValue("inputGain");
        auto* outputParam = audioProcessor.getAPVTS().getRawParameterValue("outputGain");

        if (smooshParam) audioProcessor.getAPVTS().getParameter("smoosh")->setValueNotifyingHost(preset.smoosh / 100.0f);
        if (inputParam) audioProcessor.getAPVTS().getParameter("inputGain")->setValueNotifyingHost(preset.inputGain / 30.0f);
        if (outputParam) audioProcessor.getAPVTS().getParameter("outputGain")->setValueNotifyingHost((preset.outputGain + 12.0f) / 24.0f);
    }
}
