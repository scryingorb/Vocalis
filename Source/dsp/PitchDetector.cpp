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
    maxLag = std::min(capacity - window - 2, static_cast<int>(std::ceil(analysisRate / 65.0)));
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
        const float a = difference[static_cast<size_t>(lag - 1)], b = difference[static_cast<size_t>(lag)], c = difference[static_cast<size_t>(lag + 1)];
        const float denominator = a - 2 * b + c;
        const float offset = denominator > 1e-20f ? std::clamp(0.5f * (a - c) / denominator, -0.5f, 0.5f) : 0;
        const float hz = static_cast<float>(analysisRate / (lag + offset));
        if (hz >= 65 && hz <= 1100) current = {hz, 1 - normalized[static_cast<size_t>(lag)]};
        return;
    }
}
