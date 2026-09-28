#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Control mapping for the existing spectral engine, not a second audio stage.
class FormantProcessor final
{
public:
    struct Controls { float shift = 0.0f, preserve = 1.0f; };
    struct Settings { float semitones; bool compensatePitch; };
    void prepare(double sampleRate, int analysisLatency, float shift, float preservePercent);
    void reset(float shift, float preservePercent) noexcept;
    void setTargets(float shift, float preservePercent) noexcept;
    Controls next() noexcept;
    bool shiftIsNeutral() const noexcept;
    static Settings settings(float pitch, Controls controls) noexcept;
    // Fixed envelope-width reference, not a detected fundamental.
    static float analysisReference(double sampleRate) noexcept { return static_cast<float>(200.0 / sampleRate); }
private:
    juce::SmoothedValue<float> shiftSmooth, preserveSmooth;
    std::vector<Controls> history;
    size_t position = 0;
};
