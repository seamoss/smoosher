#pragma once

#include "SmoosherDSP.h"

// A repeatable macro, independent of Normal's parameters and preset bank.
// Drive, Hammer, parallel blend, and fixed output compensation rise together.
namespace DestroyCurve
{
inline SmoosherDSP::Controls settings (float amount)
{
    struct Point { float amount; SmoosherDSP::Controls controls; };
    static constexpr std::array<Point, 6> points {{
        {   0, {   0,    0,   0,   0, 100,  90 } },
        {  15, {  25, 4.5f,  -3,  35, 100, 100 } },
        {  30, {  48,    9,  -6,  55, 100, 100 } },
        {  50, {  72,   18,  -8,  78, 100, 120 } },
        {  75, {  90,   24,  -9,  92, 100, 100 } },
        { 100, { 100,   30, -10, 100, 100,  60 } }
    }};
    amount = std::isfinite (amount) ? juce::jlimit (0.0f, 100.0f, amount) : 0.0f;
    for (size_t i = 1; i < points.size(); ++i)
        if (amount <= points[i].amount)
        {
            const auto& a = points[i - 1];
            const auto& b = points[i];
            const auto t = (amount - a.amount) / (b.amount - a.amount);
            const auto blend = [t] (float x, float y) { return x + (y - x) * t; };
            return { blend (a.controls.smoosh, b.controls.smoosh),
                     blend (a.controls.inputDB, b.controls.inputDB),
                     blend (a.controls.outputDB, b.controls.outputDB),
                     blend (a.controls.mix, b.controls.mix),
                     blend (a.controls.stereoLink, b.controls.stereoLink),
                     blend (a.controls.detectorHPF, b.controls.detectorHPF) };
        }
    return points.back().controls;
}

inline const char* description (float amount)
{
    if (amount <= 0) return "NO DAMAGE";
    if (amount < 25) return "MINOR LIABILITY";
    if (amount < 50) return "NEIGHBOR COMPLAINT";
    if (amount < 75) return "STRUCTURAL DAMAGE";
    if (amount < 100) return "INSURANCE FRAUD";
    return "TOTAL LOSS";
}
}
