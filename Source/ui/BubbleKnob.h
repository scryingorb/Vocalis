#pragma once
#include "VocalisTheme.h"
class BubbleKnob : public juce::Slider
{
public:
    explicit BubbleKnob(juce::String unit = "st", bool signedValue = true, int decimals = 2);
    juce::String getTextFromValue(double) override;
    double getValueFromText(const juce::String&) override;
private:
    juce::String units;bool signedText;int precision;
};
