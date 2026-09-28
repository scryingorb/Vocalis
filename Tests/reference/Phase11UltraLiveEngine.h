#pragma once
#include "../../Source/dsp/PitchCorrector.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cstdint>

// Research-only monophonic TD-PSOLA candidate. Not selected by the released
// processor. Fixed scheduling delay; voiced waveform features can move by an
// epoch within that delay. See docs/LowLatency.md before considering promotion.
class Phase11UltraLiveEngine final
{
public:
    void prepare(double sampleRate,int channels,float pitch=0,float formant=0,float preserve=100,double delayMs=30);
    void reset() noexcept;
    void setSemitones(float value) noexcept;
    void setFormants(float shift,float preservePercent) noexcept;
    void setCorrectionControls(PitchCorrector::Controls c) noexcept { corrector.setControls(c); }
    void setHumanizeControls(Humanizer::Controls c) noexcept { corrector.setHumanizeControls(c); }
    void process(const float* const* input,float* const* output,int samples) noexcept;
    int latencySamples() const noexcept { return delay; }
    float detectedHz() const noexcept { return corrector.detectedHz(); }
    int targetNote() const noexcept { return corrector.targetNote(); }
    float correctionSemitones() const noexcept { return corrector.correctionSemitones(); }
    uint64_t grainCount() const noexcept { return grains; }
    uint64_t supportedSamples() const noexcept { return supported; }
    double minimumSourceAge() const noexcept { return minimumAge; }
    double maximumSourceAge() const noexcept { return maximumAge; }
private:
    float read(int channel,double absolute) const noexcept;
    void grain(double centre,double period,double ratio,double formantRatio) noexcept;
    size_t slot(int64_t absolute) const noexcept;
    double rate=48000,nextCentre=0,minimumHz=100;
    double minimumAge=1e30,maximumAge=0;
    int channels=1,delay=0,lookahead=0,capacity=0;
    int64_t time=0;
    uint64_t grains=0,supported=0;
    std::array<std::vector<float>,2> history,accumulation;
    PitchCorrector corrector;
    PitchDetector tracker;
    juce::SmoothedValue<float> pitch,formant,preserve,voicedMix;
};
