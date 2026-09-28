#pragma once
#include "VocalisTheme.h"
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter(juce::String title = {});
    void setPeak(float linearPeak); // message thread, 20 Hz
    void paint(juce::Graphics&) override;
private:
    juce::String caption;
    float level = 0;
};
