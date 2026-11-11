#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <cstring>

// Frozen RMS matching, deliberately separate from the compressor. The audio
// thread owns the accumulators; commands/results use lock-free atomics only.
class EgoCheck
{
public:
    struct Settings { float trimDB = 0; bool enabled = false, learned = false; };
    enum class Status { idle, learning, matched, limited, cancelled, noSignal };
    static constexpr float minimumTrim = -48.0f, maximumTrim = 18.0f;

    Settings settings() const { return unpack (configuration.load()); }
    uint64_t word() const { return configuration.load(); }
    Status status() const { return static_cast<Status> (activity.load() & 7u); }
    float progress() const { return acceptedSeconds.load(); }
    uint64_t completedRequest() const { return completed.load(); }
    uint64_t completedWord() const { return result.load(); }
    bool isLearning() const { return status() == Status::learning; }
    void auditionDry (bool dry) { listenDry.store (dry); }

    void setSettings (Settings s)
    {
        if (! std::isfinite (s.trimDB)) s = {};
        s.trimDB = juce::jlimit (minimumTrim, maximumTrim, s.trimDB);
        if (! s.learned) s = {};
        auto previous = configuration.load();
        while (! configuration.compare_exchange_weak (previous, pack (s, nextRevision (previous)))) {}
        activity.store (tag (word(), Status::idle));
    }

    // Called by parameter listeners, including host automation on the audio thread.
    uint64_t invalidate()
    {
        auto previous = configuration.load();
        uint64_t next;
        do { next = pack (unpack (previous), nextRevision (previous)); }
        while (! configuration.compare_exchange_weak (previous, next));
        auto expected = tag (previous, Status::learning);
        activity.compare_exchange_strong (expected, tag (previous, Status::cancelled));
        return next;
    }

    uint64_t learn()
    {
        const auto token = invalidate();
        acceptedSeconds.store (0);
        requested.store (token);
        activity.store (tag (token, Status::learning));
        // An automation callback may arrive between publishing the request and
        // its status. Do not leave an already-invalid request stuck on Learning.
        if (word() != token)
        {
            auto expected = tag (token, Status::learning);
            activity.compare_exchange_strong (expected, tag (token, Status::cancelled));
        }
        return token;
    }

    void cancel()
    {
        invalidate();
        activity.store (tag (word(), Status::cancelled));
    }

    void prepare (double rate)
    {
        if (isLearning()) cancel();
        sampleRate = rate;
        windowSize = juce::jmax (1, static_cast<int> (std::round (rate * 0.1)));
        seenRequest = requested.load();
        measuring = false;
        gain.reset (rate, 0.02);
        gain.setCurrentAndTargetValue (targetGain());
        dryBlend.reset (rate, 0.02);
        dryBlend.setCurrentAndTargetValue (0);
        listenDry.store (false);
    }

    void beginBlock()
    {
        gain.setTargetValue (targetGain());
        dryBlend.setTargetValue (listenDry.load() ? 1.0f : 0.0f);
        const auto request = requested.load();
        if (request != seenRequest && request == word() && activity.load() == tag (request, Status::learning))
        {
            seenRequest = request;
            measuring = true;
            elapsed = samplesInWindow = acceptedWindows = 0;
            inputWindow = outputWindow = inputTotal = outputTotal = 0;
        }
        if (measuring && word() != seenRequest)
            measuring = false;
    }

    float nextGain() { return gain.getNextValue(); }
    float nextDryBlend() { return dryBlend.getNextValue(); }

    // Powers are channel-averaged, taken before correction and dry audition.
    void measure (double inputPower, double outputPower)
    {
        if (! measuring) return;
        ++elapsed;
        // Let the compressor/control ramps settle before taking the first window.
        if (elapsed <= static_cast<int> (sampleRate * 0.25)) return;
        inputWindow += inputPower;
        outputWindow += outputPower;
        if (++samplesInWindow == windowSize)
        {
            if (std::isfinite (inputWindow) && std::isfinite (outputWindow)
                && inputWindow / windowSize >= 1.0e-6 && outputWindow / windowSize >= 1.0e-12)
            {
                inputTotal += inputWindow;
                outputTotal += outputWindow;
                ++acceptedWindows;
                acceptedSeconds.store (static_cast<float> (acceptedWindows) * 0.1f);
            }
            samplesInWindow = 0;
            inputWindow = outputWindow = 0;
            if (acceptedWindows >= 40)
            {
                const auto db = static_cast<float> (10.0 * std::log10 (inputTotal / outputTotal));
                const auto trim = juce::jlimit (minimumTrim, maximumTrim, db);
                auto expected = seenRequest;
                const auto newWord = pack ({ trim, true, true }, nextRevision (expected));
                if (configuration.compare_exchange_strong (expected, newWord))
                {
                    gain.setTargetValue (juce::Decibels::decibelsToGain (trim));
                    result.store (newWord);
                    completed.store (seenRequest);
                    finish (db < minimumTrim || db > maximumTrim ? Status::limited : Status::matched);
                }
                else finish (Status::cancelled);
                measuring = false;
            }
        }
        if (measuring && elapsed >= static_cast<int> (sampleRate * 20.0))
        {
            // Do not overwrite the status of a newer command.
            if (word() == seenRequest) finish (Status::noSignal);
            measuring = false;
        }
    }

private:
    static_assert (std::atomic<uint64_t>::is_always_lock_free, "Ego Check needs lock-free 64-bit atomics");
    static uint64_t tag (uint64_t token, Status s) { return ((token >> 34) << 3) | static_cast<uint64_t> (s); }
    void finish (Status s)
    {
        auto expected = tag (seenRequest, Status::learning);
        activity.compare_exchange_strong (expected, tag (seenRequest, s));
    }
    static uint64_t nextRevision (uint64_t value) { return ((value >> 34) + 1) & 0x3fffffffu; }
    static uint64_t pack (Settings s, uint64_t revision)
    {
        uint32_t bits;
        std::memcpy (&bits, &s.trimDB, sizeof (bits));
        return bits | (uint64_t (s.enabled) << 32) | (uint64_t (s.learned) << 33) | (revision << 34);
    }
    static Settings unpack (uint64_t value)
    {
        const auto bits = static_cast<uint32_t> (value);
        Settings s;
        std::memcpy (&s.trimDB, &bits, sizeof (bits));
        s.enabled = (value & (uint64_t (1) << 32)) != 0;
        s.learned = (value & (uint64_t (1) << 33)) != 0;
        return s;
    }
    float targetGain() const
    {
        const auto s = settings();
        return s.enabled && s.learned ? juce::Decibels::decibelsToGain (s.trimDB) : 1.0f;
    }
    std::atomic<uint64_t> configuration { 0 }, requested { 0 }, completed { 0 }, result { 0 };
    std::atomic<uint64_t> activity { 0 };
    std::atomic<float> acceptedSeconds { 0 };
    std::atomic<bool> listenDry { false };
    juce::SmoothedValue<float> gain, dryBlend;
    double sampleRate = 44100, inputWindow = 0, outputWindow = 0, inputTotal = 0, outputTotal = 0;
    int windowSize = 4410, elapsed = 0, samplesInWindow = 0, acceptedWindows = 0;
    uint64_t seenRequest = 0;
    bool measuring = false;
};
