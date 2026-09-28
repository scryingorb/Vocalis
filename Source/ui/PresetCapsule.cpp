#include "PresetCapsule.h"
PresetCapsule::PresetCapsule(const juce::String& name) : BubbleButton(name)
{
    setTooltip("Load factory sound; keep Key and Scale. Manual edits remain available. A matching sound glows.");
}
