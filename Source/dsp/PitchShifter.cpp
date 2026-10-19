#include "PitchShifter.h"
#include "FormantProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <signalsmith-stretch/signalsmith-stretch.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include <array>
#include <vector>

struct PitchShifter::Impl
{
    static constexpr int quantum = 64;
    signalsmith::stretch::SignalsmithStretch<float> engine{0};
    signalsmith::stretch::SignalsmithStretch<float> rightEngine{0};
    bool independentStereo = false;
    std::array<std::array<float, quantum>, 2> input{}, output{};
    std::array<std::vector<float>, 2> unityDelay;
    std::vector<float> pitchHistory, correctionHistory;
    size_t correctionPosition = 0;
    FormantProcessor formants;
    PitchCorrector corrector;
    juce::SmoothedValue<float> pitch, shiftedMix;
    int channels = 0, latency = 0, position = 0, delayPosition = 0, pitchPosition = 0;
    int zeroSamples = 0, settleSamples = 0;
    double tail = 0.0;
};

PitchShifter::PitchShifter() : impl(std::make_unique<Impl>()) {}
PitchShifter::~PitchShifter() = default;

PitchShifter::Configuration PitchShifter::configurationFor(Mode mode, double sampleRate) noexcept
{
    // Benchmarked compromise: short window, dense overlap for low vocal notes.
    // QUALITY retains the Phase 2 configuration. See docs/LatencyOptimization.md.
    // At high rates, spread spectral work over the hop instead of doing it in
    // one callback. This costs one 3ms hop of reported/aligned latency.
    return mode == Mode::live ? Configuration{0.048, 0.003, sampleRate >= 88200.0, false}
                             : Configuration{0.12, 0.015, false};
}

void PitchShifter::prepare(double sampleRate, int channels, float initialSemitones, Mode mode,
                           float formantShift, float preservePercent)
{
    prepare(sampleRate, channels, initialSemitones, configurationFor(mode, sampleRate), formantShift, preservePercent);
}

void PitchShifter::prepare(double sampleRate, int channels, float initialSemitones, Configuration config,
                           float formantShift, float preservePercent)
{
    auto& s = *impl;
    s.channels = channels;
    // Short-window shared peak maps merge dissimilar L/R notes. In LIVE, use
    // independent maps on the same clock/control trajectory. Identical inputs
    // use identical deterministic engines; other stereo images can change.
    // QUALITY retains linked processing.
    s.independentStereo = channels == 2 && !config.linkedStereo;
    s.engine.configure(s.independentStereo ? 1 : channels, static_cast<int>(sampleRate * config.blockSeconds),
        static_cast<int>(sampleRate * config.intervalSeconds), config.splitComputation);
    if (s.independentStereo)
        s.rightEngine.configure(1, static_cast<int>(sampleRate * config.blockSeconds),
            static_cast<int>(sampleRate * config.intervalSeconds), config.splitComputation);
    s.latency = s.engine.inputLatency() + s.engine.outputLatency() + Impl::quantum;
    s.settleSamples = s.latency + s.engine.blockSamples();
    s.tail = static_cast<double>(s.settleSamples) / sampleRate;
    for (auto& delay : s.unityDelay) delay.assign(static_cast<size_t>(s.latency), 0.0f);
    s.pitchHistory.assign(static_cast<size_t>(s.engine.inputLatency()), initialSemitones);
    s.corrector.prepare(sampleRate);
    // The causal detector describes audio roughly 20 ms in the past. Match
    // remaining analysis lookback, especially QUALITY's larger window, without
    // adding audio delay. Exact detector age varies with F0 and hop position.
    s.correctionHistory.assign(static_cast<size_t>(std::max(0, s.engine.inputLatency()
        - static_cast<int>(sampleRate * 0.020))), 0.0f);
    s.formants.prepare(sampleRate, s.engine.inputLatency(), formantShift, preservePercent);
    s.engine.setFormantBase(FormantProcessor::analysisReference(sampleRate));
    if (s.independentStereo) s.rightEngine.setFormantBase(FormantProcessor::analysisReference(sampleRate));
    s.pitch.reset(sampleRate, 0.05);
    s.shiftedMix.reset(sampleRate, 0.02);
    reset(initialSemitones, formantShift, preservePercent);
}

void PitchShifter::reset(float semitones, float formantShift, float preservePercent) noexcept
{
    auto& s = *impl;
    if (s.latency == 0) return;
    // All vectors retain the capacity allocated by prepare().
    s.corrector.reset();
    s.correctionPosition = 0;
    std::fill(s.correctionHistory.begin(), s.correctionHistory.end(), 0.0f);
    s.engine.reset();
    s.engine.setTransposeSemitones(semitones);
    s.formants.reset(formantShift, preservePercent);
    const auto initialFormant = FormantProcessor::settings(semitones, {formantShift, preservePercent * 0.01f});
    s.engine.setFormantSemitones(initialFormant.semitones, initialFormant.compensatePitch);
    if (s.independentStereo)
    {
        s.rightEngine.reset();
        s.rightEngine.setTransposeSemitones(semitones);
        s.rightEngine.setFormantSemitones(initialFormant.semitones, initialFormant.compensatePitch);
    }
    s.position = s.delayPosition = s.pitchPosition = 0;
    s.input = {}; s.output = {};
    for (auto& delay : s.unityDelay) std::fill(delay.begin(), delay.end(), 0.0f);
    std::fill(s.pitchHistory.begin(), s.pitchHistory.end(), semitones);
    s.pitch.setCurrentAndTargetValue(semitones);
    const bool neutral = semitones == 0.0f && formantShift == 0.0f;
    s.shiftedMix.setCurrentAndTargetValue(neutral ? 0.0f : 1.0f);
    s.zeroSamples = neutral ? s.settleSamples : 0;
}

void PitchShifter::setSemitones(float semitones) noexcept
{
    impl->pitch.setTargetValue(juce::jlimit(-12.0f, 12.0f, semitones));
}

void PitchShifter::setHumanizeControls(Humanizer::Controls controls) noexcept
{
    impl->corrector.setHumanizeControls(controls);
}

void PitchShifter::setCorrectionControls(PitchCorrector::Controls controls) noexcept
{
    impl->corrector.setControls(controls);
}
float PitchShifter::detectedHz() const noexcept { return impl->corrector.detectedHz(); }

void PitchShifter::setFormants(float semitones, float preservePercent) noexcept
{
    impl->formants.setTargets(semitones, preservePercent);
}

void PitchShifter::process(const float* const* input, float* const* output, int samples) noexcept
{
    auto& s = *impl;
    for (int i = 0; i < samples; ++i)
    {
        const float pitch = s.pitch.getNextValue();
        // A causal estimate already describes preceding audio. Do not send it
        // through the manual automation history and add another 24 ms of lag.
        // Stereo uses the left vocal channel as the shared tuning reference.
        float correction = s.corrector.processSample(input[0][i]);
        if (!s.correctionHistory.empty())
        {
            const float delayed = s.correctionHistory[s.correctionPosition];
            s.correctionHistory[s.correctionPosition] = correction;
            if (++s.correctionPosition == s.correctionHistory.size()) s.correctionPosition = 0;
            correction = delayed;
        }
        const auto formant = s.formants.next();
        // Automation follows the centre of the analysis window, not its newest input.
        const float enginePitch = s.pitchHistory[static_cast<size_t>(s.pitchPosition)] + correction;
        s.pitchHistory[static_cast<size_t>(s.pitchPosition)] = pitch;
        if (++s.pitchPosition == static_cast<int>(s.pitchHistory.size())) s.pitchPosition = 0;

        if (pitch == 0.0f && correction == 0.0f && !s.pitch.isSmoothing() && s.formants.shiftIsNeutral())
            s.zeroSamples = juce::jmin(s.zeroSamples + 1, s.settleSamples);
        else
            s.zeroSamples = 0;
        // Keep processing during bypass/zero: never restart the spectral engine
        // in an audio callback. Wait for its history to settle before returning
        // to exact unity, and crossfade instead of switching samples abruptly.
        s.shiftedMix.setTargetValue(s.zeroSamples >= s.settleSamples ? 0.0f : 1.0f);
        const float blend = s.shiftedMix.getNextValue();
        for (int ch = 0; ch < s.channels; ++ch)
        {
            const auto c = static_cast<size_t>(ch);
            const float clean = s.unityDelay[c][static_cast<size_t>(s.delayPosition)];
            s.unityDelay[c][static_cast<size_t>(s.delayPosition)] = input[ch][i];
            s.input[c][static_cast<size_t>(s.position)] = input[ch][i];
            output[ch][i] = blend == 0.0f ? clean
                : clean + blend * (s.output[c][static_cast<size_t>(s.position)] - clean);
        }
        if (++s.delayPosition == s.latency) s.delayPosition = 0;
        if (++s.position == Impl::quantum)
        {
            s.position = 0;
            const float* in[] = {s.input[0].data(), s.input[1].data()};
            float* out[] = {s.output[0].data(), s.output[1].data()};
            s.engine.setTransposeSemitones(enginePitch);
            const auto setting = FormantProcessor::settings(enginePitch, formant);
            s.engine.setFormantSemitones(setting.semitones, setting.compensatePitch);
            s.engine.process(in, Impl::quantum, out, Impl::quantum);
            if (s.independentStereo)
            {
                s.rightEngine.setTransposeSemitones(enginePitch);
                s.rightEngine.setFormantSemitones(setting.semitones, setting.compensatePitch);
                s.rightEngine.process(in + 1, Impl::quantum, out + 1, Impl::quantum);
            }
        }
    }
}

int PitchShifter::latencySamples() const noexcept { return impl->latency; }
double PitchShifter::tailSeconds() const noexcept { return impl->tail; }
