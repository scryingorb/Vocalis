#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Control-domain humanization. No audio buffer, RNG or extra spectral stage.
class Humanizer final
{
public:
    struct Controls { float amount = 0, vibrato = 0, drift = 0, smoothness = 0; };
    void prepare(double analysisIntervalSeconds) noexcept;
    void reset() noexcept;
    void setControls(Controls) noexcept;
    void advance() noexcept; // once per detector update, including unvoiced updates
    void observe(float midiNote, bool voiced, bool newTarget) noexcept;
    float hardness(float baseHardness) const noexcept;
    double transitionSeconds() const noexcept;
    double retuneSeconds(double baseSeconds) const noexcept;
    float shapeCorrection(float errorSemitones) const noexcept;
private:
    float humanize() const noexcept { return controls[0].getCurrentValue(); }
    float effective(size_t index, float macroContribution) const noexcept;
    std::array<juce::SmoothedValue<float>, 4> controls;
    float driftFollow = 0, driftAnchor = 0, settled = 0;
    float followAlpha = 0, anchorAlpha = 0, settleAlpha = 0;
    bool voicedPreviously = false;
};
