#include "Humanizer.h"
#include <cmath>

void Humanizer::prepare(double interval) noexcept
{
    for (auto& control : controls) control.reset(1.0 / interval, 0.05);
    followAlpha = static_cast<float>(1 - std::exp(-interval / 0.35));
    anchorAlpha = static_cast<float>(1 - std::exp(-interval / 1.5));
    settleAlpha = static_cast<float>(1 - std::exp(-interval / 0.25));
    reset();
}
void Humanizer::reset() noexcept
{
    for (auto& control : controls) control.setCurrentAndTargetValue(control.getTargetValue());
    driftFollow = driftAnchor = settled = 0;
    voicedPreviously = false;
}
void Humanizer::setControls(Controls values) noexcept
{
    const float targets[] = {values.amount, values.vibrato, values.drift, values.smoothness};
    for (size_t i = 0; i < controls.size(); ++i)
        controls[i].setTargetValue(juce::jlimit(0.0f, 1.0f, targets[i] * 0.01f));
}
void Humanizer::advance() noexcept
{
    for (auto& control : controls) control.getNextValue();
}
void Humanizer::observe(float note, bool voiced, bool newTarget) noexcept
{
    if (!voiced) { voicedPreviously = false; settled = 0; return; }
    if (!voicedPreviously || newTarget)
    {
        driftFollow = driftAnchor = note;
        settled = 0;
    }
    voicedPreviously = true;
    driftFollow += followAlpha * (note - driftFollow);
    driftAnchor += anchorAlpha * (driftFollow - driftAnchor);
    settled += settleAlpha * (1 - settled);
}
float Humanizer::effective(size_t index, float macroContribution) const noexcept
{
    const float macro = humanize() * macroContribution;
    return macro + (1 - macro) * controls[index].getCurrentValue();
}
float Humanizer::hardness(float base) const noexcept
{
    return base * (1 - effective(1, 0.7f));
}
double Humanizer::transitionSeconds() const noexcept
{
    return 0.060 * effective(3, 0.6f);
}
double Humanizer::retuneSeconds(double base) const noexcept
{
    // More easing around a new note, less on the settled portion of a sustain.
    return base + 0.090 * effective(3, 0.6f) * (1 - 0.6f * settled);
}
float Humanizer::shapeCorrection(float error) const noexcept
{
    const float width = 0.08f * humanize() * (0.5f + 0.5f * settled);
    // Soft centres keep up to 8 cents of real deviation. Increasing width over
    // the sustain relaxes correction strength without an independent oscillator.
    if (width > 0) error -= width * std::tanh(error / width);
    // Preserve only slow measured movement, bounded to +/-4 cents. A perfectly
    // steady input settles to zero drift. Reacquisition never reuses old drift.
    if (voicedPreviously)
        error += 0.04f * effective(2, 0.5f) * std::tanh((driftFollow - driftAnchor) / 0.04f);
    return error;
}
