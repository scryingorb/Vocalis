#include "PitchDetector.h"
#include <algorithm>
#include <cmath>

void PitchDetector::prepare(double sampleRate, Profile profile) noexcept
{
    // Opt-in experiment only: halve integration/hop while retaining the low-F0
    // lag search. This reduces acquisition delay, not the causal period needed.
    window = profile == Profile::fastExperimental ? 128 : 256;
    hopSamples = profile == Profile::fastExperimental ? 32 : 64;
    decimation = std::max(1, static_cast<int>(std::ceil(sampleRate / 12000.0)));
    analysisRate = sampleRate / decimation;
    minLag = std::max(2, static_cast<int>(analysisRate / 1100.0));
    // Retain neighbours beyond the lowest supported period so its valley and
    // parabolic interpolation are not truncated at the search boundary.
    maxLag = std::min(capacity - window - 2, static_cast<int>(std::ceil(analysisRate / 65.0)) + 2);
    filterCoefficient = static_cast<float>(1 - std::exp(-6.283185307179586 * 1800 / sampleRate));
    reset();
}
void PitchDetector::reset() noexcept
{
    ring.fill(0); frame.fill(0); difference.fill(0); normalized.fill(0); filter.fill(0);
    write = filled = decimationPosition = hop = 0;
    current = {};
}
bool PitchDetector::processSample(float sample) noexcept
{
    if (!std::isfinite(sample)) sample = 0;
    for (auto& stage : filter) { stage += filterCoefficient * (sample - stage); sample = stage; }
    if (++decimationPosition < decimation) return false;
    decimationPosition = 0;
    ring[static_cast<size_t>(write)] = sample;
    write = (write + 1) % capacity;
    filled = std::min(filled + 1, capacity);
    if (++hop < hopSamples) return false;
    hop = 0;
    if (filled >= window + maxLag + 1) analyse();
    else current = {};
    return true;
}
void PitchDetector::analyse() noexcept
{
    const int length = window + maxLag + 1;
    for (int i = 0; i < length; ++i)
        frame[static_cast<size_t>(i)] = ring[static_cast<size_t>((write - length + i + capacity) % capacity)];
    double energy = 0;
    for (int i = maxLag; i < maxLag + window; ++i) energy += frame[static_cast<size_t>(i)] * frame[static_cast<size_t>(i)];
    current = {};
    if (energy / window < 1e-7) return; // filtered RMS below approximately -70 dBFS
    double sum = 0;
    normalized[0] = 1;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double d = 0;
        for (int i = maxLag; i < maxLag + window; ++i)
        {
            const double delta = frame[static_cast<size_t>(i)] - frame[static_cast<size_t>(i - lag)];
            d += delta * delta;
        }
        difference[static_cast<size_t>(lag)] = static_cast<float>(d);
        sum += d;
        normalized[static_cast<size_t>(lag)] = sum > 1e-20 ? static_cast<float>(d * lag / sum) : 1.0f;
    }
    // First sufficiently periodic valley rejects many octave-down alternatives.
    for (int lag = minLag; lag < maxLag - 1; ++lag)
    {
        if (normalized[static_cast<size_t>(lag)] >= 0.15f) continue;
        while (lag + 1 < maxLag && normalized[static_cast<size_t>(lag + 1)] < normalized[static_cast<size_t>(lag)]) ++lag;
        if (lag >= maxLag) return;
        // A dominant second harmonic can make a half-period valley pass YIN's
        // threshold. Prefer the full period only when it explains substantially
        // more of the waveform. Pure tones already have a near-zero first valley
        // and must never be arbitrarily shifted down an octave.
        const auto valleyScore = [this](int index)
        {
            const float a=difference[static_cast<size_t>(index-1)],b=difference[static_cast<size_t>(index)],c=difference[static_cast<size_t>(index+1)];
            const float curvature=a-2*b+c;
            const float offset=curvature>1e-20f?std::clamp(.5f*(a-c)/curvature,-.5f,.5f):0;
            // Remove integer-lag sampling error before comparing periodicity.
            // Otherwise a high pure tone with a fractional period can look more
            // periodic at twice its period and be incorrectly halved.
            return b>1e-20f?normalized[static_cast<size_t>(index)]*std::max(0.0f,b-.25f*(a-c)*offset)/b:0.0f;
        };
        const float firstScore=valleyScore(lag);
        // Restrict disambiguation to low/mid vocals: at high frequencies the
        // decimated waveform cannot reliably distinguish interpolation error
        // from evidence of a missing fundamental. Keep the original YIN choice.
        if (analysisRate / lag <= 600.0 && firstScore > 0.01f && 2 * lag + 2 < maxLag)
        {
            int fullPeriod = 2 * lag;
            for (int candidate = 2 * lag - 2; candidate <= 2 * lag + 2; ++candidate)
                if (valleyScore(candidate) < valleyScore(fullPeriod))
                    fullPeriod = candidate;
            if (valleyScore(fullPeriod) < 0.2f * firstScore)
                lag = fullPeriod;
        }
        const float a = difference[static_cast<size_t>(lag - 1)], b = difference[static_cast<size_t>(lag)], c = difference[static_cast<size_t>(lag + 1)];
        const float denominator = a - 2 * b + c;
        const float offset = denominator > 1e-20f ? std::clamp(0.5f * (a - c) / denominator, -0.5f, 0.5f) : 0;
        const float hz = static_cast<float>(analysisRate / (lag + offset));
        // Interpolation can overshoot an inclusive endpoint by a few cents,
        // depending on the decimated rate. Allow less than nine cents of edge
        // error, then keep the published output range at 65-1100 Hz.
        constexpr float boundaryTolerance = 1.005f;
        if (hz >= 65 / boundaryTolerance && hz <= 1100 * boundaryTolerance)
            current = {std::clamp(hz, 65.0f, 1100.0f), 1 - normalized[static_cast<size_t>(lag)]};
        return;
    }
}
