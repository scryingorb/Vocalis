#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Decoded once when the editor is constructed, never on the audio thread.
class VocalisLogo final : public juce::Component
{
public:
    VocalisLogo();
    void paint(juce::Graphics&) override;
private:
    juce::Image image;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalisLogo)
};
