#pragma once
#include "VocalisTheme.h"
class BubbleButton : public juce::TextButton
{
public:
    explicit BubbleButton(const juce::String& text, bool toggle = false);
    void paintButton(juce::Graphics&,bool,bool) override;
};
