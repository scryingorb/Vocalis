#pragma once
#include "VocalisTheme.h"
class PitchOrb final : public juce::Component
{
public:
    PitchOrb();
    void setFrequency(float hz, bool enabled);
    void paint(juce::Graphics&) override;
    juce::String noteText() const {return note;}
    juce::String centsText() const {return cents;}
private:
    juce::String note="--", cents="--";
};
