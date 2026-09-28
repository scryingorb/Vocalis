#include "FormantProcessor.h"

void FormantProcessor::prepare(double sampleRate, int analysisLatency, float shift, float preservePercent)
{
    history.resize(static_cast<size_t>(analysisLatency));
    shiftSmooth.reset(sampleRate, 0.05);
    preserveSmooth.reset(sampleRate, 0.05);
    reset(shift, preservePercent);
}

void FormantProcessor::reset(float shift, float preservePercent) noexcept
{
    shiftSmooth.setCurrentAndTargetValue(juce::jlimit(-6.0f, 6.0f, shift));
    preserveSmooth.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, preservePercent * 0.01f));
    std::fill(history.begin(), history.end(), Controls{shiftSmooth.getCurrentValue(), preserveSmooth.getCurrentValue()});
    position = 0;
}

void FormantProcessor::setTargets(float shift, float preservePercent) noexcept
{
    shiftSmooth.setTargetValue(juce::jlimit(-6.0f, 6.0f, shift));
    preserveSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, preservePercent * 0.01f));
}

FormantProcessor::Controls FormantProcessor::next() noexcept
{
    const auto delayed = history[position];
    history[position] = {shiftSmooth.getNextValue(), preserveSmooth.getNextValue()};
    if (++position == history.size()) position = 0;
    return delayed;
}

bool FormantProcessor::shiftIsNeutral() const noexcept
{
    return shiftSmooth.getCurrentValue() == 0.0f && !shiftSmooth.isSmoothing();
}

FormantProcessor::Settings FormantProcessor::settings(float pitch, Controls controls) noexcept
{
    // Native compensation is Boolean. Interpolate the desired final envelope
    // position in semitones instead of crossfading two spectral processors.
    // Target = explicit shift + the unpreserved fraction of natural pitch motion.
    if (controls.shift - controls.preserve * pitch == 0.0f)
        return {0.0f, false}; // Identity envelope correction; exact Phase 2.5 path.
    return {controls.shift + (1.0f - controls.preserve) * pitch, true};
}
