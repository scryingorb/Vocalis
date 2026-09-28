#pragma once
#include <memory>
#include "PitchCorrector.h"

// Streaming, equal-duration pitch shift. All storage is allocated in prepare().
// Input/output must be separate, planar, and contain the prepared channel count.
class PitchShifter final
{
public:
    enum class Mode { live, quality };
    struct Configuration
    {
        double blockSeconds;
        double intervalSeconds;
        bool splitComputation = false;
        bool linkedStereo = true;
    };
    static Configuration configurationFor(Mode) noexcept;
    PitchShifter();
    ~PitchShifter();
    // Configuration changes are preparation-only, never automatable in process().
    void prepare(double sampleRate, int channels, float initialSemitones, Mode mode = Mode::live,
                 float formantShift = 0.0f, float preservePercent = 100.0f);
    void prepare(double sampleRate, int channels, float initialSemitones, Configuration,
                 float formantShift = 0.0f, float preservePercent = 100.0f);
    void reset(float semitones, float formantShift = 0.0f, float preservePercent = 100.0f) noexcept;
    void setSemitones(float semitones) noexcept;
    void setHumanizeControls(Humanizer::Controls) noexcept;
    void setCorrectionControls(PitchCorrector::Controls) noexcept;
    float detectedHz() const noexcept;
    void setFormants(float semitones, float preservePercent) noexcept;
    void process(const float* const* input, float* const* output, int samples) noexcept;
    int latencySamples() const noexcept;
    double tailSeconds() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
