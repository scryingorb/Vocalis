#include "PitchCorrector.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool PitchCorrector::allowed(int note, int key, int scale) noexcept
{
    if (scale == 2) return true;
    const int degree = ((note - key) % 12 + 12) % 12;
    constexpr unsigned major = (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11);
    constexpr unsigned minor = (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10);
    return ((scale == 1 ? minor : major) & (1u << degree)) != 0;
}
int PitchCorrector::nearestNote(float note, int key, int scale) noexcept
{
    const int middle = static_cast<int>(std::floor(note));
    int best = middle; float distance = std::numeric_limits<float>::max();
    for (int n = middle - 12; n <= middle + 12; ++n)
        if (allowed(n, key, scale) && std::abs(note - n) < distance)
        { best = n; distance = std::abs(note - n); }
    return best; // Exact ties favour the lower note.
}
void PitchCorrector::prepare(double sampleRate, PitchDetector::Profile profile) noexcept
{
    rate = sampleRate; detector.prepare(rate, profile);
    humanizer.prepare(detector.updateSeconds());
    releaseCoefficient = static_cast<float>(1 - std::exp(-1 / (rate * 0.025)));
    setControls(controls); reset();
}
void PitchCorrector::reset() noexcept
{
    humanizer.reset();
    detector.reset(); correction = desired = centre = 0;
    target = candidate = -1; candidateFrames = voicedFrames = 0; tracking = false;
}
void PitchCorrector::setControls(Controls c) noexcept
{
    c.key = std::clamp(c.key, 0, 11); c.scale = std::clamp(c.scale, 0, 2);
    c.amount = std::clamp(c.amount, 0.0f, 100.0f); c.speed = std::clamp(c.speed, 0.0f, 100.0f);
    c.transition = std::clamp(c.transition, 0.0f, 100.0f); c.hard = std::clamp(c.hard, 0.0f, 100.0f);
    if (c.key != controls.key || c.scale != controls.scale) { target = candidate = -1; candidateFrames = 0; }
    if (c.amount == 0 && controls.amount != 0)
    { detector.reset(); humanizer.observe(0, false, false); desired = 0; tracking = false; voicedFrames = 0; target = candidate = -1; }
    controls = c;
    // Time constant: 250 ms at Speed 0, 4 ms at Speed 100. No zero-time steps.
    baseRetuneSeconds = 0.25 * std::pow(0.004 / 0.25, c.speed * 0.01);
    updateRetuneCoefficient();
}
void PitchCorrector::updateRetuneCoefficient() noexcept
{
    retuneCoefficient = static_cast<float>(1 - std::exp(-1 / (rate * humanizer.retuneSeconds(baseRetuneSeconds))));
}
void PitchCorrector::update() noexcept
{
    humanizer.advance();
    const auto result = detector.result();
    if (result.hz == 0 || result.confidence < 0.85f)
    {
        humanizer.observe(0, false, false);
        updateRetuneCoefficient();
        desired = 0; tracking = false; voicedFrames = candidateFrames = 0; target = candidate = -1;
        return;
    }
    const float note = 69 + 12 * std::log2(result.hz / 440.0f);
    // Only acquisition needs a count; saturation avoids overflow on long sessions.
    voicedFrames = std::min(voicedFrames + 1, 2);
    if (voicedFrames < 2) return; // avoid a one-frame acquisition
    if (!tracking) { centre = note; tracking = true; }
    const float hard = humanizer.hardness(controls.hard * 0.01f);
    const float centreAlpha = static_cast<float>(1 - std::exp(-detector.updateSeconds() / 0.12));
    centre += centreAlpha * (note - centre);
    const int nearest = nearestNote(note, controls.key, controls.scale);
    bool newTarget = target < 0;
    if (newTarget) target = nearest;
    // Note Transition controls target persistence. Natural adds boundary hysteresis.
    if (nearest != target && std::abs(note - target) > std::abs(note - nearest) + 0.3f * (1 - hard))
    {
        if (candidate != nearest) { candidate = nearest; candidateFrames = 0; }
        const double hold = controls.transition * 0.001 * (1 - 0.9 * hard) + humanizer.transitionSeconds();
        if (++candidateFrames * detector.updateSeconds() >= hold) { newTarget = true; target = nearest; centre = note; candidate = -1; candidateFrames = 0; }
    }
    else { candidate = -1; candidateFrames = 0; }
    // Natural removes slow centre error, leaving faster vibrato in the signal.
    // Hard progressively follows instantaneous F0 and flattens that modulation.
    const float correctionReference = centre + hard * (note - centre);
    humanizer.observe(note, true, newTarget);
    updateRetuneCoefficient();
    desired = std::clamp(humanizer.shapeCorrection(std::clamp(target - correctionReference, -3.0f, 3.0f)),
        -3.0f, 3.0f) * controls.amount * 0.01f;
}
float PitchCorrector::processSample(float input) noexcept
{
    if (controls.amount > 0)
    {
        if (detector.processSample(input)) update();
    }
    else { desired = 0; if (tracking || voicedFrames != 0) { detector.reset(); tracking = false; voicedFrames = 0; target = candidate = -1; } }
    const float coefficient = tracking && controls.amount > 0 ? retuneCoefficient : releaseCoefficient;
    correction += coefficient * (desired - correction);
    if (desired == 0 && std::abs(correction) < 1e-6f) correction = 0;
    return correction;
}
