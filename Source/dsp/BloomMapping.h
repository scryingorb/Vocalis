#pragma once
#include "Humanizer.h"

// Edit the macro voicing here. Base APVTS values remain independently editable.
namespace BloomMapping
{
struct Result { Humanizer::Controls humanizer; float formant; };
inline Result apply(float percent, Humanizer::Controls base, float formant) noexcept
{
    const float b = juce::jlimit(0.0f, 1.0f, percent * 0.01f);
    base.amount += (100 - base.amount) * 0.60f * b;
    base.vibrato += (100 - base.vibrato) * 0.50f * b;
    base.smoothness += (100 - base.smoothness) * 0.55f * b;
    return {base, juce::jlimit(-6.0f, 6.0f, formant + 0.75f * b)};
}
}
