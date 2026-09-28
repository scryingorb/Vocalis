#pragma once
#include "VocalisTheme.h"
class SectionPanel final : public juce::Component
{
public:
    explicit SectionPanel(juce::String title = {});
    void paint(juce::Graphics&) override;
    juce::Rectangle<int> contentBounds() const;
private:
    juce::String heading;
};
