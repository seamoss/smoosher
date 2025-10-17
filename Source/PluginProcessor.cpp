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
    currentSampleRate = sampleRate;
    envelope.resize(getTotalNumOutputChannels(), 0.0f);
    hpState1.resize(getTotalNumOutputChannels(), 0.0f);
    hpState2.resize(getTotalNumOutputChannels(), 0.0f);
    lpState.resize(getTotalNumOutputChannels(), 0.0f);
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

void SmoosherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get parameter values
    float smooshAmount = apvts.getRawParameterValue("smoosh")->load();
    float inputGainDB = apvts.getRawParameterValue("inputGain")->load();
    float outputGainDB = apvts.getRawParameterValue("outputGain")->load();
    float mixAmount = apvts.getRawParameterValue("mix")->load() / 100.0f; // Convert to 0-1 range

    // Convert dB to linear gain
    float inputGain = juce::Decibels::decibelsToGain(inputGainDB);
    float outputGain = juce::Decibels::decibelsToGain(outputGainDB);

    // Map the single smoosh knob to compressor parameters
    // Smoosh 0% = no compression, no effect
    // Smoosh 60% = what was previously 100%
    // Smoosh 60-100% = EXTRA aggressive "hammer" mode
    float normalizedSmoosh = smooshAmount / 100.0f;

    // Only process if smoosh is above 0.1%
    bool compressionActive = smooshAmount > 0.1f;

    // Determine if we're in "hammer" mode (60-100%)
    bool hammerMode = smooshAmount > 60.0f;
    float hammerAmount = hammerMode ? (smooshAmount - 60.0f) / 40.0f : 0.0f; // 0-1 for 60-100% range

    // Remap 0-60% to be like old 0-100%
    float remappedSmoosh = hammerMode ? 1.0f : (normalizedSmoosh / 0.6f);

    // Threshold: starts very high at 0%, goes to -12 dB at 60%, then -6.4 dB at 100%
    // (Reduced by ~20% for less aggressive compression)
    float threshold;
    if (hammerMode)
    {
        // In hammer mode: go from -12 dB to -6.4 dB (lower threshold = more compression)
        threshold = juce::jmap(hammerAmount, -12.0f, -6.4f);
    }
    else
    {
        // Normal mode: 0 to -12 dB with exponential curve
        threshold = juce::jmap(remappedSmoosh * remappedSmoosh, 0.0f, -12.0f);
    }

    // Ratio: 1:1 at 0% → 10:1 at 60% → 16:1 at 100%
    // (Reduced by ~20% for less aggressive compression)
    float ratio;
    if (hammerMode)
    {
        // In hammer mode: go from 10:1 to 16:1
        ratio = juce::jmap(hammerAmount, 10.0f, 16.0f);
    }
    else
    {
        // Normal mode: 1:1 to 10:1
        ratio = juce::jmap(remappedSmoosh, 1.0f, 10.0f);
    }

    // Attack: 10ms at 0% → 3ms at 60% → 1ms at 100% (super fast in hammer mode)
    float attackMs;
    if (hammerMode)
    {
        // In hammer mode: go from 3ms to 1ms (very fast)
        attackMs = juce::jmap(hammerAmount, 3.0f, 1.0f);
    }
    else
    {
        // Normal mode: 10ms to 3ms
        attackMs = juce::jmap(remappedSmoosh, 10.0f, 3.0f);
    }

    // Release: 100ms at 0% → 200ms at 60% → 150ms at 100% (slightly faster in hammer mode)
    float releaseMs;
    if (hammerMode)
    {
        // In hammer mode: go from 200ms to 150ms (faster release for punchiness)
        releaseMs = juce::jmap(hammerAmount, 200.0f, 150.0f);
    }
    else
    {
        // Normal mode: 100ms to 200ms
        releaseMs = juce::jmap(remappedSmoosh, 100.0f, 200.0f);
    }

    // Calculate attack and release coefficients
    float attackCoeff = std::exp(-1.0f / (currentSampleRate * attackMs / 1000.0f));
    float releaseCoeff = std::exp(-1.0f / (currentSampleRate * releaseMs / 1000.0f));

    // Convert threshold to linear
    float thresholdLinear = juce::Decibels::decibelsToGain(threshold);

    // Makeup gain: 1.0 at 0% → 2.0 at 60% → 2.8 at 100%
    // (Reduced by ~20% for less aggressive output)
    float makeupGain;
    if (hammerMode)
    {
        // In hammer mode: 2.0 to 2.8
        makeupGain = juce::jmap(hammerAmount, 2.0f, 2.8f);
    }
    else
    {
        // Normal mode: 1.0 to 2.0 with smooth curve
        makeupGain = 1.0f + (remappedSmoosh * remappedSmoosh * 1.0f);
    }

    // Saturation: keep it controlled to avoid harsh distortion
    // 0 at 0% → 0.24 at 60% → 0.28 at 100%
    // (Reduced by ~20% for less coloration)
    float saturationAmount;
    if (hammerMode)
    {
        saturationAmount = juce::jmap(hammerAmount, 0.24f, 0.28f);
    }
    else
    {
        saturationAmount = remappedSmoosh * 0.24f;
    }

    // Sibilance sensitivity
    float sibilanceSensitivity = normalizedSmoosh;

    // High-pass filter coefficient for sibilance detection
    float hpFreq = juce::jmap(normalizedSmoosh, 2000.0f, 5000.0f);
    float hpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * hpFreq / static_cast<float>(currentSampleRate));

    // Low-pass filter: more aggressive in hammer mode to prevent harshness
    // Normal: 20kHz → 8kHz, Hammer: 8kHz → 6kHz
    float lpFreq;
    if (hammerMode)
    {
        lpFreq = juce::jmap(hammerAmount, 8000.0f, 6000.0f);
    }
    else
    {
        lpFreq = juce::jmap(remappedSmoosh, 20000.0f, 8000.0f);
    }
    float lpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * lpFreq / static_cast<float>(currentSampleRate));

    // Process each channel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Store dry signal for wet/dry mixing
            float drySample = channelData[sample];

            // Apply input gain
            float inputSample = channelData[sample] * inputGain;

            float processedSample = inputSample;

            if (compressionActive)
            {
                // Apply tube-style saturation (soft clipping with harmonic coloration)
                if (saturationAmount > 0.001f)
                {
                    float saturated = inputSample * (1.0f + saturationAmount);
                    // Soft clipping with tanh for tube-like saturation
                    saturated = std::tanh(saturated * 1.5f) / 1.5f;
                    // Blend between clean and saturated based on saturation amount
                    processedSample = inputSample + (saturated - inputSample) * saturationAmount * 2.0f;
                }

                // Low-pass filter to reduce high-frequency harshness
                lpState[channel] = processedSample * (1.0f - lpCoeff) + lpState[channel] * lpCoeff;
                processedSample = lpState[channel];

                // High-pass filter for sibilance detection (simple one-pole)
                float hpSample = processedSample - hpState1[channel];
                hpState1[channel] = processedSample * (1.0f - hpCoeff) + hpState1[channel] * hpCoeff;

                // Second stage for steeper roll-off
                float hpSample2 = hpSample - hpState2[channel];
                hpState2[channel] = hpSample * (1.0f - hpCoeff) + hpState2[channel] * hpCoeff;

                // Blend between full-spectrum and high-passed for sidechain detection
                float detectionSample = processedSample + (hpSample2 - processedSample) * sibilanceSensitivity;

                // Get absolute value for envelope detection
                float inputLevel = std::abs(detectionSample);

                // Envelope follower (peak detection)
                if (inputLevel > envelope[channel])
                    envelope[channel] = attackCoeff * envelope[channel] + (1.0f - attackCoeff) * inputLevel;
                else
                    envelope[channel] = releaseCoeff * envelope[channel] + (1.0f - releaseCoeff) * inputLevel;

                // Calculate gain reduction
                float gainReduction = 1.0f;
                if (envelope[channel] > thresholdLinear)
                {
                    // Calculate how much we're over the threshold
                    float overThresholdDB = juce::Decibels::gainToDecibels(envelope[channel] / thresholdLinear);

                    // Apply ratio for compression
                    float gainReductionDB = overThresholdDB * (1.0f - 1.0f / ratio);

                    // Convert back to linear
                    gainReduction = juce::Decibels::decibelsToGain(-gainReductionDB);
                }

                // Apply compression and makeup gain
                processedSample = processedSample * gainReduction * makeupGain;

                // Soft limiting to prevent harsh clipping in hammer mode
                if (hammerMode)
                {
                    // Soft clip at ±0.95 to prevent hard clipping
                    float limitThreshold = 0.95f;
                    if (std::abs(processedSample) > limitThreshold)
                    {
                        float sign = processedSample > 0.0f ? 1.0f : -1.0f;
                        float excess = std::abs(processedSample) - limitThreshold;
                        // Soft knee limiting
                        processedSample = sign * (limitThreshold + std::tanh(excess * 2.0f) * 0.05f);
                    }
                }
            }

            // Apply output gain to wet signal
            float wetSample = processedSample * outputGain;

            // Blend dry and wet signals based on mix amount
            channelData[sample] = drySample * (1.0f - mixAmount) + wetSample * mixAmount;
        }
    }
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
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SmoosherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmoosherAudioProcessor();
}
