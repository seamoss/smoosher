#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SmoosherAudioProcessor::SmoosherAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    smooshParameter = apvts.getRawParameterValue ("smoosh");
    inputParameter = apvts.getRawParameterValue ("inputGain");
    outputParameter = apvts.getRawParameterValue ("outputGain");
    mixParameter = apvts.getRawParameterValue ("mix");
}

SmoosherAudioProcessor::~SmoosherAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SmoosherAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Single main compression knob (0-100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "smoosh",
        "Smoosh",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }
    ));

    // Input gain (0 to +30 dB, default 0 = unity gain)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "inputGain",
        "Input",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }
    ));

    // Output gain trim (-12 to +12 dB, center = 0)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "outputGain",
        "Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }
    ));

    // Mix control (0-100%, wet/dry blend)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "%"; }
    ));

    return layout;
}

//==============================================================================
const juce::String SmoosherAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SmoosherAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SmoosherAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SmoosherAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SmoosherAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SmoosherAudioProcessor::getNumPrograms()
{
    return 1;
}

int SmoosherAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SmoosherAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SmoosherAudioProcessor::getProgramName (int index)
{
    return {};
}

void SmoosherAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SmoosherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    dsp.prepare (sampleRate, controls());
    meterLeft.store (0); meterRight.store (0); meterReduction.store (0);
    outputClipped.store (false);
}

void SmoosherAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SmoosherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

SmoosherDSP::Controls SmoosherAudioProcessor::controls() const
{
    return { smooshParameter->load(), inputParameter->load(), outputParameter->load(), mixParameter->load() };
}

void SmoosherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
    const auto meters = dsp.process (buffer, controls());
    auto accumulate = [] (std::atomic<float>& destination, float value)
    {
        auto previous = destination.load (std::memory_order_relaxed);
        while (previous < value && ! destination.compare_exchange_weak (previous, value, std::memory_order_relaxed)) {}
    };
    accumulate (meterLeft, meters.left);
    accumulate (meterRight, meters.right);
    accumulate (meterReduction, meters.reductionDB);
    if (meters.clipped)
        outputClipped.store (true, std::memory_order_relaxed);
}

SmoosherDSP::MeterReadings SmoosherAudioProcessor::consumeMeters()
{
    return { meterLeft.exchange (0), meterRight.exchange (0), meterReduction.exchange (0), outputClipped.load() };
}

//==============================================================================
bool SmoosherAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SmoosherAudioProcessor::createEditor()
{
    return new SmoosherAudioProcessorEditor (*this);
}

//==============================================================================
void SmoosherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto preset = getCurrentPreset();
    state.setProperty ("presetName", preset.name, nullptr);
    state.setProperty ("presetSmoosh", preset.smoosh, nullptr);
    state.setProperty ("presetInput", preset.inputDB, nullptr);
    state.setProperty ("presetOutput", preset.outputDB, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SmoosherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xmlState);
    apvts.replaceState (state);
    Preset preset { state.getProperty ("presetName").toString(),
                    static_cast<float> (state.getProperty ("presetSmoosh")),
                    static_cast<float> (state.getProperty ("presetInput")),
                    static_cast<float> (state.getProperty ("presetOutput")) };
    if (! state.hasProperty ("presetSmoosh") || ! state.hasProperty ("presetInput")
        || ! state.hasProperty ("presetOutput") || ! validPreset (preset))
        preset = inferPreset();
    const juce::ScopedLock lock (presetLock);
    currentPreset = preset;
}

//==============================================================================
const std::array<SmoosherAudioProcessor::Preset, 17>& SmoosherAudioProcessor::factoryPresets()
{
    static const std::array<Preset, 17> presets {{
        { "Init", 0, 0, 0 }, { "Hell Smarsh", 45, 15, 1 },
        { "Wenger's 7-String", 60, 10.5f, 2 }, { "That Joe Sound", 70, 15, 0 },
        { "Gravy River", 70, 12, 2 }, { "Dani Time", 30, 30, 2 },
        { "Gloopy", 40, 7.5f, 1 }, { "Limit THIS!", 75, 24, 3 },
        { "Mr. Cetera", 24, 6, -3 },
        { "Dockside Divorce", 38, 7.5f, -4.5f },
        { "Drum Bus", 48, 6, -4 },
        { "Bass in Boat Shoes", 42, 9, -5 },
        { "Snare Custody Battle", 66, 12, -6 },
        { "Oops! All Room Mics", 92, 24, -9 },
        { "Live, Laugh, Limit", 85, 18, -7 },
        { "Divorced Dad Clean Tone", 20, 4.5f, -3 },
        { juce::String::fromUTF8 (u8"Hard Dick\u2122"), 73, 12, -5.5f }
    }};
    return presets;
}

bool SmoosherAudioProcessor::validPreset (const Preset& p)
{
    return p.name.isNotEmpty() && p.name.length() <= 64
        && std::isfinite (p.smoosh) && p.smoosh >= 0 && p.smoosh <= 100
        && std::isfinite (p.inputDB) && p.inputDB >= 0 && p.inputDB <= 30
        && std::isfinite (p.outputDB) && p.outputDB >= -12 && p.outputDB <= 12;
}

void SmoosherAudioProcessor::loadPreset (const Preset& preset)
{
    if (! validPreset (preset))
        return;
    auto setParameter = [this] (const char* id, float value)
    {
        auto* parameter = apvts.getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    };
    setParameter ("smoosh", preset.smoosh);
    setParameter ("inputGain", preset.inputDB);
    setParameter ("outputGain", preset.outputDB);
    const juce::ScopedLock lock (presetLock);
    currentPreset = preset;
}

SmoosherAudioProcessor::Preset SmoosherAudioProcessor::getCurrentPreset() const
{
    const juce::ScopedLock lock (presetLock);
    return currentPreset;
}

bool SmoosherAudioProcessor::isPresetModified() const
{
    const auto p = getCurrentPreset();
    const auto c = controls();
    return std::abs (p.smoosh - c.smoosh) > 0.051f
        || std::abs (p.inputDB - c.inputDB) > 0.051f
        || std::abs (p.outputDB - c.outputDB) > 0.051f;
}

SmoosherAudioProcessor::Preset SmoosherAudioProcessor::inferPreset() const
{
    const auto c = controls();
    for (const auto& p : factoryPresets())
        if (std::abs (p.smoosh - c.smoosh) < 0.051f
            && std::abs (p.inputDB - c.inputDB) < 0.051f
            && std::abs (p.outputDB - c.outputDB) < 0.051f)
            return p;
    return { "Custom", c.smoosh, c.inputDB, c.outputDB };
}

juce::File SmoosherAudioProcessor::userPresetDirectory()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    directory = directory.getChildFile ("Application Support");
   #endif
    return directory.getChildFile ("Helvete Sound/Smoosher/Presets");
}

juce::Result SmoosherAudioProcessor::saveUserPreset (const juce::File& file, const juce::String& name)
{
    const auto c = controls();
    Preset preset { name.trim(), c.smoosh, c.inputDB, c.outputDB };
    if (! validPreset (preset))
        return juce::Result::fail ("Enter a preset name of 1 to 64 characters.");
    if (file.exists())
        return juce::Result::fail ("A preset with that name already exists. Choose a new name.");
    auto result = file.getParentDirectory().createDirectory();
    if (result.failed())
        return result;
    juce::XmlElement xml ("SmoosherPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", preset.name);
    xml.setAttribute ("smoosh", preset.smoosh);
    xml.setAttribute ("input", preset.inputDB);
    xml.setAttribute ("output", preset.outputDB);
    juce::TemporaryFile temporary (file);
    if (! xml.writeTo (temporary.getFile()) || ! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The preset could not be saved to this folder.");
    const juce::ScopedLock lock (presetLock);
    currentPreset = preset;
    return juce::Result::ok();
}

juce::Result SmoosherAudioProcessor::loadUserPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("SmoosherPreset") || xml->getIntAttribute ("version") != 1
        || ! xml->hasAttribute ("smoosh") || ! xml->hasAttribute ("input") || ! xml->hasAttribute ("output"))
        return juce::Result::fail ("This file is not a supported Smoosher preset.");
    Preset preset { xml->getStringAttribute ("name"),
                    static_cast<float> (xml->getDoubleAttribute ("smoosh")),
                    static_cast<float> (xml->getDoubleAttribute ("input")),
                    static_cast<float> (xml->getDoubleAttribute ("output")) };
    if (! validPreset (preset))
        return juce::Result::fail ("This preset contains invalid parameter values.");
    loadPreset (preset);
    return juce::Result::ok();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmoosherAudioProcessor();
}
