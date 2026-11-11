#pragma once

#include <JuceHeader.h>
#include <array>
#include "EgoCheck.h"

// The 1.0 processing curves, with a 20 ms ramp between control settings.
// Smoothers advance once per sample, shared by both channels.
class SmoosherDSP
{
public:
    struct Controls { float smoosh, inputDB, outputDB, mix; };
    struct MeterReadings { float left = 0, right = 0, reductionDB = 0; bool clipped = false; };

    void prepare (double rate, Controls controls)
    {
        sampleRate = rate;
        envelope.fill (0); hp1.fill (0); hp2.fill (0); lp.fill (0);
        const auto values = targets (controls);
        for (size_t i = 0; i < smooth.size(); ++i)
        {
            smooth[i].reset (rate, 0.02);
            smooth[i].setCurrentAndTargetValue (values[i]);
        }
    }

    MeterReadings process (juce::AudioBuffer<float>& buffer, Controls controls, EgoCheck* ego = nullptr)
    {
        if (ego != nullptr) ego->beginBlock();
        const auto values = targets (controls);
        for (size_t i = 0; i < smooth.size(); ++i)
            smooth[i].setTargetValue (values[i]);

        MeterReadings meters;
        const auto channels = juce::jmin (2, buffer.getNumChannels());
        auto* const* data = buffer.getArrayOfWritePointers();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto matchGain = ego != nullptr ? ego->nextGain() : 1.0f;
            const auto dryBlend = ego != nullptr ? ego->nextDryBlend() : 0.0f;
            double inputPower = 0, outputPower = 0;
            std::array<float, count> v;
            for (size_t i = 0; i < smooth.size(); ++i)
                v[i] = smooth[i].getNextValue();

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto c = static_cast<size_t> (channel);
                const auto dry = data[channel][sample];
                const auto input = dry * v[inputGain];
                auto processed = input;

                if (v[active] > 0.0f)
                {
                    if (v[saturation] > 0.001f)
                    {
                        auto saturated = input * (1.0f + v[saturation]);
                        saturated = std::tanh (saturated * 1.5f) / 1.5f;
                        processed = input + (saturated - input) * v[saturation] * 2.0f;
                    }

                    lp[c] = processed * (1.0f - v[lowpass]) + lp[c] * v[lowpass];
                    processed = lp[c];
                    const auto high = processed - hp1[c];
                    hp1[c] = processed * (1.0f - v[highpass]) + hp1[c] * v[highpass];
                    const auto high2 = high - hp2[c];
                    hp2[c] = high * (1.0f - v[highpass]) + hp2[c] * v[highpass];
                    const auto detector = processed + (high2 - processed) * v[sibilance];
                    const auto level = std::abs (detector);
                    const auto coefficient = level > envelope[c] ? v[attack] : v[release];
                    envelope[c] = coefficient * envelope[c] + (1.0f - coefficient) * level;

                    float reduction = 0.0f;
                    if (envelope[c] > v[threshold])
                        reduction = juce::Decibels::gainToDecibels (envelope[c] / v[threshold]) * v[ratioFactor];
                    meters.reductionDB = juce::jmax (meters.reductionDB, reduction * v[active]);
                    processed = processed * juce::Decibels::decibelsToGain (-reduction) * v[makeup];

                    if (v[hammer] > 0.0f && std::abs (processed) > 0.95f)
                    {
                        const auto limited = std::copysign (0.95f + std::tanh ((std::abs (processed) - 0.95f) * 2.0f) * 0.05f, processed);
                        processed = v[hammer] == 1.0f ? limited : processed + (limited - processed) * v[hammer];
                    }
                    if (v[active] < 1.0f)
                        processed = input + (processed - input) * v[active];
                }

                auto output = dry * (1.0f - v[blend]) + processed * v[outputGain] * v[blend];
                inputPower += static_cast<double> (dry) * dry;
                outputPower += static_cast<double> (output) * output;
                if (matchGain != 1.0f) output *= matchGain;
                if (dryBlend != 0.0f) output += (dry - output) * dryBlend;
                data[channel][sample] = output;
                auto& peak = channel == 0 ? meters.left : meters.right;
                peak = juce::jmax (peak, std::abs (output));
                meters.clipped = meters.clipped || std::abs (output) >= 1.0f;
            }
            if (ego != nullptr && channels > 0) ego->measure (inputPower / channels, outputPower / channels);
        }
        if (channels == 1)
            meters.right = meters.left;
        return meters;
    }

private:
    enum Setting { inputGain, outputGain, blend, threshold, ratioFactor, attack, release,
                   makeup, saturation, sibilance, highpass, lowpass, active, hammer, count };
    std::array<juce::SmoothedValue<float>, count> smooth;
    std::array<float, 2> envelope {}, hp1 {}, hp2 {}, lp {};
    double sampleRate = 44100.0;

    std::array<float, count> targets (Controls c) const
    {
        const auto normal = c.smoosh / 100.0f;
        const auto isHammer = c.smoosh > 60.0f;
        const auto hammerAmount = isHammer ? (c.smoosh - 60.0f) / 40.0f : 0.0f;
        const auto remapped = isHammer ? 1.0f : normal / 0.6f;
        const auto thresholdDB = isHammer ? juce::jmap (hammerAmount, -12.0f, -6.4f)
                                          : juce::jmap (remapped * remapped, 0.0f, -12.0f);
        const auto ratio = isHammer ? juce::jmap (hammerAmount, 10.0f, 16.0f) : juce::jmap (remapped, 1.0f, 10.0f);
        const auto attackMS = isHammer ? juce::jmap (hammerAmount, 3.0f, 1.0f) : juce::jmap (remapped, 10.0f, 3.0f);
        const auto releaseMS = isHammer ? juce::jmap (hammerAmount, 200.0f, 150.0f) : juce::jmap (remapped, 100.0f, 200.0f);
        const auto lpFrequency = isHammer ? juce::jmap (hammerAmount, 8000.0f, 6000.0f)
                                          : juce::jmap (remapped, 20000.0f, 8000.0f);
        return {
            juce::Decibels::decibelsToGain (c.inputDB),
            juce::Decibels::decibelsToGain (c.outputDB),
            c.mix / 100.0f,
            juce::Decibels::decibelsToGain (thresholdDB),
            1.0f - 1.0f / ratio,
            static_cast<float> (std::exp (-1.0 / (sampleRate * attackMS / 1000.0))),
            static_cast<float> (std::exp (-1.0 / (sampleRate * releaseMS / 1000.0))),
            isHammer ? juce::jmap (hammerAmount, 2.0f, 2.8f) : 1.0f + remapped * remapped,
            isHammer ? juce::jmap (hammerAmount, 0.24f, 0.28f) : remapped * 0.24f,
            normal,
            std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmap (normal, 2000.0f, 5000.0f) / static_cast<float> (sampleRate)),
            std::exp (-2.0f * juce::MathConstants<float>::pi * lpFrequency / static_cast<float> (sampleRate)),
            c.smoosh > 0.1f ? 1.0f : 0.0f,
            isHammer ? 1.0f : 0.0f
        };
    }
};
