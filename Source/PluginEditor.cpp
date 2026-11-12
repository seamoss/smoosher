#include "PluginEditor.h"

namespace
{
const juce::Colour background { 0xff171c1d }, panel { 0xff202728 }, text { 0xffecf1e9 };
const juce::Colour muted { 0xff9aa8a6 }, green { 0xffb7df80 }, amber { 0xffefb76d }, red { 0xfff07167 };
juce::String levelText (float db) { return db <= -60.0f ? "-inf" : juce::String (db, 1); }
constexpr float inputCornerRadius = 6.0f;
}

SmoosherLookAndFeel::SmoosherLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff121718));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff374242));
    setColour (juce::Slider::textBoxHighlightColourId, green.withAlpha (0.3f));
    setColour (juce::Slider::thumbColourId, green);
    setColour (juce::Slider::trackColourId, green);
    setColour (juce::Slider::backgroundColourId, juce::Colour (0xff35403e));
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3b4846));
    setColour (juce::ComboBox::arrowColourId, green);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff40513d));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff303b37));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::buttonOnColourId, green);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, green);
    setColour (juce::ToggleButton::tickDisabledColourId, muted);
    setColour (juce::Label::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
}

juce::Label* SmoosherLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setColour (juce::Label::outlineWhenEditingColourId, green);
    label->onEditorShow = [label]
    {
        if (auto* editor = label->getCurrentTextEditor())
            editor->setOpaque (false);
    };
    return label;
}

void SmoosherLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr)
    {
        juce::LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    const auto bounds = label.getLocalBounds().toFloat().reduced (0.5f);
    const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
    g.setColour (label.findColour (juce::Label::backgroundColourId));
    g.fillRoundedRectangle (bounds, inputCornerRadius);
    if (! label.isBeingEdited())
    {
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (getLabelFont (label));
        g.drawFittedText (label.getText(), getLabelBorderSize (label).subtractedFrom (label.getLocalBounds()),
                          label.getJustificationType(), 1, label.getMinimumHorizontalScale());
        g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (bounds, inputCornerRadius, 1.0f);
    }
}

void SmoosherLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height)).reduced (0.5f),
                            inputCornerRadius);
}

void SmoosherLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId)
                      .withMultipliedAlpha (editor.isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle (juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height)).reduced (0.75f),
                            inputCornerRadius, focused ? 1.5f : 1.0f);
}

void SmoosherLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float position, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                               static_cast<float> (width), static_cast<float> (height)).reduced (10);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto isDestroy = slider.getName() == "Destroy";
    const auto colour = slider.getName() == "Smoosh" || isDestroy ? green.interpolatedWith (red, position * 0.9f) : green;
    juce::Path track, fill;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0, startAngle, endAngle, true);
    fill.addCentredArc (centre.x, centre.y, radius, radius, 0, startAngle, angle, true);
    g.setColour (juce::Colour (0xff35413d));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (fill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto face = juce::Rectangle<float> (radius * 1.65f, radius * 1.65f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff394341), face.getTopLeft(),
                                            juce::Colour (0xff202726), face.getBottomRight(), false));
    g.fillEllipse (face);
    g.setColour (juce::Colour (0xff4c5852));
    g.drawEllipse (face, 1.0f);
    const auto tipRadius = radius * (isDestroy ? 0.74f : 0.66f);
    const auto baseRadius = radius * (isDestroy ? 0.58f : 0.38f);
    const juce::Point<float> tip (centre.x + std::sin (angle) * tipRadius,
                                 centre.y - std::cos (angle) * tipRadius);
    const juce::Point<float> base (centre.x + std::sin (angle) * baseRadius,
                                  centre.y - std::cos (angle) * baseRadius);
    g.setColour (text);
    g.drawLine ({ base, tip }, 3.0f);
    if (isDestroy)
    {
        g.setColour (colour);
        g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
        g.drawText (juce::String (slider.getValue(), 1) + "%",
                    face.withHeight (40).withCentre ({ centre.x, centre.y + 12.0f }),
                    juce::Justification::centred);
    }
}

SmoosherAudioProcessorEditor::SmoosherAudioProcessorEditor (SmoosherAudioProcessor& processor)
    : AudioProcessorEditor (&processor), audioProcessor (processor)
{
    setLookAndFeel (&lookAndFeel);
    configureKnob (inputGainSlider, inputGainLabel, "Input", "Drive into the compressor. Double-click to reset to 0 dB.");
    configureKnob (smooshSlider, smooshLabel, "Smoosh", "Compression and colour. Above 60% enters Hammer mode. Double-click to reset.");
    configureKnob (outputGainSlider, outputGainLabel, "Output", "Wet output trim. Double-click to reset to 0 dB.");
    mixSlider.setName ("Mix");
    mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 65, 24);
    mixSlider.setTooltip ("Blend the dry input with the wet signal. Presets leave Mix unchanged. Double-click for 100% wet.");
    addAndMakeVisible (mixSlider);
    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centredRight);
    mixLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    mixLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (mixLabel);

    linkSlider.setName ("Couples Therapy - Stereo Link");
    detectorSlider.setName ("Kick Privilege - Detector Bass Filter");
    for (auto* slider : { &linkSlider, &detectorSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 88, 24);
        addChildComponent (slider);
    }
    linkSlider.setTooltip ("Couples Therapy: 0% is independent stereo; 100% applies the stronger gain reduction to both channels. Mono is unchanged.");
    detectorSlider.setTooltip ("Kick Privilege: Off or 20-300 Hz. Reduces bass in the detector, not the audio path. Double-click for Off.");
    auto& state = audioProcessor.getAPVTS();
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    smooshAttachment = std::make_unique<Attachment> (state, "smoosh", smooshSlider);
    inputGainAttachment = std::make_unique<Attachment> (state, "inputGain", inputGainSlider);
    outputGainAttachment = std::make_unique<Attachment> (state, "outputGain", outputGainSlider);
    mixAttachment = std::make_unique<Attachment> (state, "mix", mixSlider);
    linkAttachment = std::make_unique<Attachment> (state, "stereoLink", linkSlider);
    detectorAttachment = std::make_unique<Attachment> (state, "detectorHPF", detectorSlider);
    destroySlider.setName ("Destroy");
    destroySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    destroySlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    destroySlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    destroySlider.setTooltip ("One knob. Calculated damage. Drag or scroll; double-click for clean. Normal settings stay saved.");
    addChildComponent (destroySlider);
    destroyAttachment = std::make_unique<Attachment> (state, "destroy", destroySlider);
    addAndMakeVisible (modeButton);
    modeButton.onClick = [this] { audioProcessor.setDestroyMode (! audioProcessor.destroyMode()); updateMoreTrouble(); };
    for (auto* slider : { &inputGainSlider, &smooshSlider, &outputGainSlider, &mixSlider, &linkSlider, &detectorSlider, &destroySlider })
    {
        slider->onDragStart = [this, slider]
        {
            ++activeGestures;
            audioProcessor.beginUserEdit (slider->getName());
        };
        slider->onDragEnd = [this]
        {
            if (activeGestures > 0) { --activeGestures; audioProcessor.endUserEdit(); }
        };
    }

    presetComboBox.setName ("Preset");
    presetComboBox.setTooltip ("Factory and user presets. An asterisk means the preset has been modified.");
    presetComboBox.onChange = [this]
    {
        const auto id = presetComboBox.getSelectedId();
        if (id > 0 && id <= static_cast<int> (SmoosherAudioProcessor::factoryPresets().size()))
            audioProcessor.loadPreset (SmoosherAudioProcessor::factoryPresets()[static_cast<size_t> (id - 1)]);
        else if (id >= 1000 && id - 1000 < userPresets.size())
        {
            const auto result = audioProcessor.loadUserPreset (userPresets.getReference (id - 1000));
            if (result.failed())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Unable to load preset", result.getErrorMessage());
        }
        updatePresetDisplay();
    };
    addAndMakeVisible (presetComboBox);
    saveButton.onClick = [this] { savePreset(); };
    saveButton.setTooltip ("Save Input, Smoosh, Output, and the Shape controls. Mix remains independent.");
    addAndMakeVisible (saveButton);
    modifiedLabel.setText ("MODIFIED", juce::dontSendNotification);
    modifiedLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    modifiedLabel.setColour (juce::Label::textColourId, amber);
    addChildComponent (modifiedLabel);
    clipButton.setTooltip ("Lights when final output reaches 0 dBFS. Click to clear the peak indication.");
    clipButton.onClick = [this]
    {
        audioProcessor.clearClip();
        heldPeak = -100.0f;
        peakHoldUntil = 0;
        timerCallback();
    };
    addAndMakeVisible (clipButton);
    addAndMakeVisible (moreButton);
    moreButton.setTooltip ("Compare, Shape, and Chaos. More tools, same questionable judgment. Included free.");
    moreButton.onClick = [this]
    {
        audioProcessor.setMoreTroubleOpen (! audioProcessor.moreTroubleOpen());
        updateMoreTrouble();
    };
    for (auto* button : { &aButton, &bButton, &copyButton, &undoButton, &redoButton,
                          &learnButton, &matchButton, &dryButton, &resetEgoButton,
                          &compareTab, &shapeTab, &chaosTab, &generateButton })
        addChildComponent (button);
    aButton.setTooltip ("Recall A: all six controls, preset identity, and Ego Check.");
    bButton.setTooltip ("Recall B. On first use, B starts as a copy of A.");
    copyButton.setTooltip ("Replace the other slot with this one. Undo can reverse the copy.");
    aButton.onClick = [this] { audioProcessor.selectSlot (0); updateMoreTrouble(); };
    bButton.onClick = [this] { audioProcessor.selectSlot (1); updateMoreTrouble(); };
    copyButton.onClick = [this] { generatedSlot = -1; audioProcessor.copyToOtherSlot(); updateMoreTrouble(); };
    undoButton.onClick = [this] { generatedSlot = -1; audioProcessor.undo(); updateMoreTrouble(); };
    redoButton.onClick = [this] { generatedSlot = -1; audioProcessor.redo(); updateMoreTrouble(); };
    learnButton.setTooltip ("Play a representative passage. Learn four seconds of audible RMS, then freeze the level correction.");
    learnButton.onClick = [this]
    {
        if (audioProcessor.egoStatus() == EgoCheck::Status::learning) audioProcessor.cancelEgo();
        else audioProcessor.learnEgo();
        updateMoreTrouble();
    };
    matchButton.setTooltip ("Enable or bypass the learned correction, applied after Mix. Relearn after changing the sound.");
    matchButton.onClick = [this] { audioProcessor.enableEgo (! audioProcessor.egoSettings().enabled); updateMoreTrouble(); };
    dryButton.setTooltip ("Hold to hear the original input. Release to hear Smoosher again.");
    dryButton.onStateChange = [this] { audioProcessor.auditionDry (dryButton.isDown() && drawerVisible && visiblePage == 0); };
    resetEgoButton.setTooltip ("Clear this slot's learned correction. Undo can bring it back.");
    resetEgoButton.onClick = [this] { audioProcessor.resetEgo(); updateMoreTrouble(); };
    egoStatusLabel.setFont (juce::FontOptions (11.0f));
    egoStatusLabel.setColour (juce::Label::textColourId, muted);
    egoGainLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    egoGainLabel.setJustificationType (juce::Justification::centredRight);
    egoGainLabel.setColour (juce::Label::textColourId, green);
    addChildComponent (egoStatusLabel);
    addChildComponent (egoGainLabel);
    compareTab.onClick = [this] { audioProcessor.setMoreTroublePage (0); updateMoreTrouble(); };
    shapeTab.onClick = [this] { audioProcessor.setMoreTroublePage (1); updateMoreTrouble(); };
    chaosTab.onClick = [this] { audioProcessor.setMoreTroublePage (2); updateMoreTrouble(); };
    chaosComboBox.setName ("Bad Decisions amount");
    chaosComboBox.addItemList ({ "Gentle", "Questionable", "Unsupervised" }, 1);
    chaosComboBox.setTooltip ("Gentle makes small moves, Questionable makes wider moves, and Unsupervised explores the full unlocked ranges.");
    chaosComboBox.onChange = [this] { audioProcessor.setChaosAmount (chaosComboBox.getSelectedId() - 1); };
    addChildComponent (chaosComboBox);
    const std::array<juce::String, 6> names {{ "Smoosh", "Input", "Output", "Mix", "Link", "Bass" }};
    for (size_t i = 0; i < lockButtons.size(); ++i)
    {
        auto& button = lockButtons[i];
        button.setButtonText (names[i]);
        button.setName ("Lock " + names[i]);
        button.setTooltip ("Checked means locked: keep " + names[i] + " unchanged in the generated variation.");
        button.onClick = [this, i] { audioProcessor.setChaosLocked (static_cast<int> (i), lockButtons[i].getToggleState()); };
        addChildComponent (button);
    }
    generateButton.setTooltip ("Replace the inactive slot with a variation of this sound. The active sound stays put. Undo recovers the replaced slot.");
    generateButton.onClick = [this]
    {
        if (audioProcessor.generateVariation()) generatedSlot = 1 - audioProcessor.activeSlot();
        updateMoreTrouble();
    };
    chaosStatusLabel.setFont (juce::FontOptions (11.0f));
    chaosStatusLabel.setColour (juce::Label::textColourId, muted);
    addChildComponent (chaosStatusLabel);
    refreshPresetList();
    setSize (620, 470);
    updateMoreTrouble();
    lastMeterTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

SmoosherAudioProcessorEditor::~SmoosherAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.auditionDry (false);
    while (activeGestures > 0) { --activeGestures; audioProcessor.endUserEdit(); }
    setLookAndFeel (nullptr);
}

void SmoosherAudioProcessorEditor::configureKnob (juce::Slider& slider, juce::Label& label,
                                                 const juce::String& name, const juce::String& tooltip)
{
    slider.setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 24);
    slider.setScrollWheelEnabled (true);
    slider.setTooltip (tooltip);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void SmoosherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    if (destroyVisible)
    {
        g.setColour (muted);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText ("SMOOSHER", 24, 22, 180, 24, juce::Justification::centredLeft);
        g.setColour (green.interpolatedWith (red, static_cast<float> (destroySlider.getValue()) * 0.009f));
        g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
        g.drawText ("DESTROY", 24, 80, 372, 38, juce::Justification::centred);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (DestroyCurve::description (static_cast<float> (destroySlider.getValue())),
                    24, 414, 372, 24, juce::Justification::centred);
        return;
    }
    g.setColour (green);
    g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    g.drawText ("SMOOSHER", 24, 15, 240, 32, juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("HELVETE SOUND  /  CHARACTER COMPRESSOR", 25, 45, 350, 17, juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JucePlugin_VersionString), 516, 24, 80, 20, juce::Justification::centredRight);
    g.setColour (juce::Colour (0xff303a37));
    g.drawHorizontalLine (65, 24, 596);
    if (smooshSlider.getValue() > 60.0)
    {
        g.setColour (amber.withAlpha (0.15f));
        g.fillRoundedRectangle (270, 303, 80, 18, 4);
        g.setColour (amber);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("HAMMER", 270, 303, 80, 18, juce::Justification::centred);
    }
    drawMeters (g);
    g.setColour (muted);
    g.setFont (juce::FontOptions (10.0f));
    const auto ego = audioProcessor.egoSettings();
    const auto slot = audioProcessor.activeSlot() == 0 ? "A" : "B";
    const auto shaped = linkSlider.getValue() > 0 || detectorSlider.getValue() >= 20;
    g.drawText (juce::String (slot) + (ego.enabled ? " / EGO ON" : "") + (shaped ? " / SHAPE" : ""),
                204, 424, 190, 20, juce::Justification::centredLeft);
    if (drawerVisible)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (24, 477, 572, 223, 8);
        g.setColour (muted);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("BAD DECISIONS INSURANCE", 367, 489, 215, 18, juce::Justification::centredRight);
        g.drawText (juce::String ("EDITING ") + slot, 264, 526, 110, 18, juce::Justification::centredLeft);
        if (visiblePage == 0)
        {
            g.drawText ("EGO CHECK  /  LOUDER ISN'T BETTER", 38, 570, 400, 18, juce::Justification::centredLeft);
            g.drawText ("FOUR SECONDS OF HONESTY. THEN IT STAYS PUT.", 38, 676, 544, 16, juce::Justification::centredLeft);
        }
        else if (visiblePage == 1)
        {
            g.drawText ("COUPLES THERAPY", 38, 570, 250, 18, juce::Justification::centredLeft);
            g.drawText ("KICK PRIVILEGE", 330, 570, 252, 18, juce::Justification::centredLeft);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Stereo link", 38, 593, 250, 18, juce::Justification::centredLeft);
            g.drawText ("Detector bass filter", 330, 593, 252, 18, juce::Justification::centredLeft);
            g.drawText ("Independent at 0%. Together at 100%.", 38, 676, 270, 16, juce::Justification::centredLeft);
            g.drawText ("Less kick in the detector. Same bass path.", 330, 676, 252, 16, juce::Justification::centredLeft);
        }
        else
        {
            g.drawText ("BAD DECISIONS", 38, 574, 190, 18, juce::Justification::centredLeft);
            g.drawText ("LOCKED CONTROLS", 38, 630, 270, 16, juce::Justification::centredLeft);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Output and Mix start locked. Select the other slot to audition; relearn Ego Check.",
                        38, 676, 544, 16, juce::Justification::centredLeft);
        }
        g.setColour (juce::Colour (0xff35403e));
        g.drawHorizontalLine (558, 38, 582);
    }
}

void SmoosherAudioProcessorEditor::resized()
{
    modeButton.setBounds (destroyVisible ? 268 : 392, 22, 128, 24);
    destroySlider.setBounds (55, 125, 310, 280);
    presetComboBox.setBounds (24, 77, 310, 28);
    saveButton.setBounds (344, 77, 76, 28);
    modifiedLabel.setBounds (430, 77, 110, 28);
    inputGainLabel.setBounds (34, 123, 144, 20);
    inputGainSlider.setBounds (34, 145, 144, 152);
    smooshLabel.setBounds (218, 113, 184, 20);
    smooshSlider.setBounds (218, 135, 184, 162);
    outputGainLabel.setBounds (442, 123, 144, 20);
    outputGainSlider.setBounds (442, 145, 144, 152);
    mixLabel.setBounds (414, 422, 44, 24);
    mixSlider.setBounds (440, 402, 156, 64);
    clipButton.setBounds (545, 337, 40, 18);
    moreButton.setBounds (24, 420, 164, 28);
    compareTab.setBounds (38, 486, 100, 26);
    shapeTab.setBounds (146, 486, 100, 26);
    chaosTab.setBounds (254, 486, 100, 26);
    aButton.setBounds (38, 524, 42, 26);
    bButton.setBounds (88, 524, 42, 26);
    copyButton.setBounds (140, 524, 112, 26);
    undoButton.setBounds (394, 524, 90, 26);
    redoButton.setBounds (492, 524, 90, 26);
    learnButton.setBounds (38, 601, 86, 28);
    matchButton.setBounds (132, 601, 96, 28);
    dryButton.setBounds (236, 601, 112, 28);
    resetEgoButton.setBounds (356, 601, 66, 28);
    egoGainLabel.setBounds (430, 601, 152, 28);
    egoStatusLabel.setBounds (33, 638, 554, 22);
    linkSlider.setBounds (38, 610, 250, 64);
    detectorSlider.setBounds (330, 610, 252, 64);
    chaosComboBox.setBounds (240, 569, 168, 28);
    generateButton.setBounds (420, 569, 162, 28);
    chaosStatusLabel.setBounds (33, 601, 554, 22);
    for (size_t i = 0; i < lockButtons.size(); ++i)
        lockButtons[i].setBounds (38 + static_cast<int> (i) * 91, 649, 89, 24);
}

void SmoosherAudioProcessorEditor::drawMeters (juce::Graphics& g)
{
    g.setColour (panel);
    g.fillRoundedRectangle (24, 326, 572, 71, 8);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (muted);
    g.drawText ("GAIN REDUCTION", 38, 336, 155, 18, juce::Justification::centredLeft);
    g.drawText ("OUTPUT", 327, 336, 90, 18, juce::Justification::centredLeft);
    g.setColour (amber);
    g.drawText (juce::String (displayedReduction, 1) + " dB", 205, 336, 70, 18, juce::Justification::centredRight);
    g.setColour (clipVisible ? red : text);
    g.drawText (levelText (heldPeak) + " dBFS", 425, 336, 110, 18, juce::Justification::centredRight);

    auto bar = [&g] (float x, float y, float width, float amount, juce::Colour colour)
    {
        g.setColour (juce::Colour (0xff101617));
        g.fillRoundedRectangle (x, y, width, 6, 2);
        g.setColour (colour);
        if (amount > 0)
            g.fillRoundedRectangle (x, y, width * juce::jlimit (0.0f, 1.0f, amount), 6, 2);
    };
    bar (38, 362, 237, displayedReduction / 24.0f, amber);
    const auto leftDB = juce::Decibels::gainToDecibels (displayedLeft, -60.0f);
    const auto rightDB = juce::Decibels::gainToDecibels (displayedRight, -60.0f);
    bar (327, 360, 257, (leftDB + 60.0f) / 60.0f, leftDB >= 0 ? red : green);
    bar (327, 369, 257, (rightDB + 60.0f) / 60.0f, rightDB >= 0 ? red : green);
    g.setColour (muted);
    g.setFont (juce::FontOptions (9.0f));
    const auto scaleLabel = [&g] (const juce::String& label, float x, float y, bool first, bool last)
    {
        const auto bounds = juce::Rectangle<float> (first ? x : last ? x - 28.0f : x - 14.0f, y, 28.0f, 12.0f);
        g.drawText (label, bounds, first ? juce::Justification::centredLeft
                                      : last ? juce::Justification::centredRight : juce::Justification::centred);
    };
    for (int reduction = 0; reduction <= 24; reduction += 6)
        scaleLabel (juce::String (reduction), 38.0f + 237.0f * static_cast<float> (reduction) / 24.0f,
                    375, reduction == 0, reduction == 24);
    for (const auto db : { -60, -36, -18, 0 })
        scaleLabel (juce::String (db), 327.0f + 257.0f * static_cast<float> (db + 60) / 60.0f,
                    380, db == -60, db == 0);
    g.drawText ("L", 316, 357, 9, 12, juce::Justification::centredLeft);
    g.drawText ("R", 316, 366, 9, 12, juce::Justification::centredLeft);
}

void SmoosherAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto seconds = static_cast<float> (juce::jlimit (0.0, 1.0, (now - lastMeterTime) / 1000.0));
    lastMeterTime = now;
    const auto meters = audioProcessor.consumeMeters();
    const auto decay = std::pow (10.0f, -24.0f * seconds / 20.0f);
    displayedLeft = juce::jmax (meters.left, displayedLeft * decay);
    displayedRight = juce::jmax (meters.right, displayedRight * decay);
    displayedReduction = juce::jmax (meters.reductionDB, displayedReduction - 18.0f * seconds);
    const auto peak = juce::Decibels::gainToDecibels (juce::jmax (meters.left, meters.right), -100.0f);
    if (peak >= heldPeak)
    {
        heldPeak = peak;
        peakHoldUntil = now + 1500.0;
    }
    else if (now > peakHoldUntil)
        heldPeak = juce::jmax (peak, heldPeak - 24.0f * seconds);
    if (meters.clipped != clipVisible)
    {
        clipVisible = meters.clipped;
        clipButton.setColour (juce::TextButton::buttonColourId, clipVisible ? red : juce::Colour (0xff303b37));
        clipButton.setColour (juce::TextButton::textColourOffId, clipVisible ? background : muted);
        clipButton.setButtonText (clipVisible ? "CLIP" : "PEAK");
    }
    updatePresetDisplay();
    updateMoreTrouble();
    repaint();
}

void SmoosherAudioProcessorEditor::updateMoreTrouble()
{
    audioProcessor.synchroniseHistory();
    const auto destroy = audioProcessor.destroyMode();
    const auto open = audioProcessor.moreTroubleOpen() && ! destroy;
    const auto page = audioProcessor.moreTroublePage();
    if (open != drawerVisible || page != visiblePage || destroy != destroyVisible)
    {
        drawerVisible = open;
        destroyVisible = destroy;
        visiblePage = page;
        for (auto* component : std::initializer_list<juce::Component*> {
                &inputGainSlider, &smooshSlider, &outputGainSlider, &mixSlider,
                &inputGainLabel, &smooshLabel, &outputGainLabel, &mixLabel,
                &presetComboBox, &saveButton, &clipButton, &moreButton })
            component->setVisible (! destroy);
        destroySlider.setVisible (destroy);
        modeButton.setButtonText (destroy ? "Normal view" : "Destroy");
        modeButton.setTooltip (destroy ? "Return to your saved Normal sound." : "One-knob Destroy mode. Your Normal sound stays saved.");
        for (auto* button : { &aButton, &bButton, &copyButton, &undoButton, &redoButton, &compareTab, &shapeTab, &chaosTab })
            button->setVisible (open);
        for (auto* button : { &learnButton, &matchButton, &dryButton, &resetEgoButton })
            button->setVisible (open && page == 0);
        egoStatusLabel.setVisible (open && page == 0);
        egoGainLabel.setVisible (open && page == 0);
        linkSlider.setVisible (open && page == 1);
        detectorSlider.setVisible (open && page == 1);
        chaosComboBox.setVisible (open && page == 2);
        generateButton.setVisible (open && page == 2);
        chaosStatusLabel.setVisible (open && page == 2);
        for (auto& button : lockButtons) button.setVisible (open && page == 2);
        setSize (destroy ? 420 : 620, open ? 716 : 470);
        moreButton.setButtonText (open ? "Less Trouble -" : "More Trouble +");
        repaint();
    }
    compareTab.setToggleState (page == 0, juce::dontSendNotification);
    shapeTab.setToggleState (page == 1, juce::dontSendNotification);
    chaosTab.setToggleState (page == 2, juce::dontSendNotification);
    linkSlider.setEnabled (audioProcessor.getTotalNumOutputChannels() > 1);
    const auto slot = audioProcessor.activeSlot();
    aButton.setToggleState (slot == 0, juce::dontSendNotification);
    bButton.setToggleState (slot == 1, juce::dontSendNotification);
    copyButton.setButtonText (slot == 0 ? "Copy A > B" : "Copy B > A");
    generateButton.setButtonText (slot == 0 ? "Generate > B" : "Generate > A");
    generateButton.setEnabled (audioProcessor.canGenerateVariation());
    chaosComboBox.setSelectedId (audioProcessor.chaosAmount() + 1, juce::dontSendNotification);
    for (size_t i = 0; i < lockButtons.size(); ++i)
        lockButtons[i].setToggleState (audioProcessor.chaosLocked (static_cast<int> (i)), juce::dontSendNotification);
    juce::String chaosMessage = "Generate into the other slot. Your current sound stays put.";
    if (! audioProcessor.canGenerateVariation()) chaosMessage = "Everything is locked. Unlock a control to invite trouble.";
    else if (generatedSlot >= 0)
        chaosMessage = generatedSlot == slot ? "Auditioning the generated slot. Undo can take you back."
                                            : juce::String (generatedSlot == 0 ? "A" : "B") + " is ready. Select it above to audition. Undo recovers the replaced slot.";
    chaosStatusLabel.setText (chaosMessage, juce::dontSendNotification);
    undoButton.setEnabled (audioProcessor.canUndo());
    redoButton.setEnabled (audioProcessor.canRedo());
    undoButton.setTooltip ("Undo: " + audioProcessor.undoName());
    redoButton.setTooltip ("Redo: " + audioProcessor.redoName());
    const auto settings = audioProcessor.egoSettings();
    const auto status = audioProcessor.egoStatus();
    const auto learning = status == EgoCheck::Status::learning;
    learnButton.setButtonText (learning ? "Cancel" : settings.learned ? "Relearn" : "Learn");
    matchButton.setEnabled (settings.learned);
    matchButton.setToggleState (settings.enabled, juce::dontSendNotification);
    matchButton.setButtonText (settings.enabled ? "Match on" : "Match off");
    resetEgoButton.setEnabled (settings.learned || learning);
    egoGainLabel.setText (settings.learned ? (settings.trimDB > 0 ? "+" : "") + juce::String (settings.trimDB, 1) + " dB" : "-- dB",
                         juce::dontSendNotification);
    juce::String message;
    if (learning) message = "Learning audible RMS: " + juce::String (audioProcessor.egoProgress(), 1) + " / 4.0 s. Keep the music playing.";
    else if (status == EgoCheck::Status::noSignal) message = "Not enough audible signal. Play a passage and try Learn again.";
    else if (status == EgoCheck::Status::cancelled) message = "Learning cancelled. Previous correction kept; ready when you are.";
    else if (settings.learned)
        message = (status == EgoCheck::Status::limited ? "Correction reached its limit. " : "Frozen RMS match. ")
                + juce::String (settings.enabled ? "Hold for Dry to compare; relearn after edits." : "Match is off. Your learned correction is saved.");
    else message = "Play a passage, hit Learn, and find out if it's good or just loud.";
    egoStatusLabel.setText (message, juce::dontSendNotification);
    updatePresetDisplay();
}

void SmoosherAudioProcessorEditor::refreshPresetList()
{
    presetComboBox.clear (juce::dontSendNotification);
    presetComboBox.addSectionHeading ("FACTORY");
    int id = 1;
    for (const auto& preset : SmoosherAudioProcessor::factoryPresets())
        presetComboBox.addItem (preset.name, id++);
    userPresets = SmoosherAudioProcessor::userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.smoosherpreset");
    std::sort (userPresets.begin(), userPresets.end(), [] (const auto& a, const auto& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    if (! userPresets.isEmpty())
    {
        presetComboBox.addSeparator();
        presetComboBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresets.size(); ++i)
        {
            const auto xml = juce::XmlDocument::parse (userPresets.getReference (i));
            const auto name = xml != nullptr ? xml->getStringAttribute ("name") : juce::String();
            presetComboBox.addItem (name.isNotEmpty() ? name : userPresets.getReference (i).getFileNameWithoutExtension(), 1000 + i);
        }
    }
    updatePresetDisplay();
}

void SmoosherAudioProcessorEditor::updatePresetDisplay()
{
    const auto preset = audioProcessor.getCurrentPreset();
    const auto modified = audioProcessor.isPresetModified();
    const auto name = preset.name + (modified ? " *" : "");
    if (presetComboBox.getText() != name)
        presetComboBox.setText (name, juce::dontSendNotification);
    modifiedLabel.setVisible (modified && ! destroyVisible);
}

void SmoosherAudioProcessorEditor::savePreset()
{
    auto* dialog = new juce::AlertWindow ("Save user preset", "Save Input, Smoosh, Output, and Shape controls. Mix stays independent.", juce::MessageBoxIconType::NoIcon);
    auto name = audioProcessor.getCurrentPreset().name;
    for (const auto& preset : SmoosherAudioProcessor::factoryPresets())
        if (preset.name == name)
            name = "My " + name;
    dialog->addTextEditor ("name", name, "Preset name");
    dialog->getTextEditor ("name")->setInputRestrictions (64);
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<SmoosherAudioProcessorEditor> safeThis (this);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, dialog] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;
        const auto presetName = dialog->getTextEditorContents ("name").trim();
        const auto fileName = juce::File::createLegalFileName (presetName);
        bool reserved = presetName.equalsIgnoreCase ("Custom");
        for (const auto& preset : SmoosherAudioProcessor::factoryPresets())
            reserved = reserved || preset.name.equalsIgnoreCase (presetName);
        auto saved = juce::Result::fail ("Choose a new name for your user preset.");
        if (! reserved && fileName.isNotEmpty())
            saved = safeThis->audioProcessor.saveUserPreset (SmoosherAudioProcessor::userPresetDirectory().getChildFile (fileName + ".smoosherpreset"), presetName);
        if (saved.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Unable to save preset", saved.getErrorMessage());
        else
            safeThis->refreshPresetList();
    }), true);
}
