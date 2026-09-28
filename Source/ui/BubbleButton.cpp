#include "BubbleButton.h"
using namespace VocalisTheme;
BubbleButton::BubbleButton(const juce::String& text,bool toggle) : juce::TextButton(text)
{
    setClickingTogglesState(toggle);setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void BubbleButton::paintButton(juce::Graphics& g,bool hover,bool down)
{
    auto r=getLocalBounds().toFloat().reduced(3);
    capsule(g,r,getToggleState(),hover,down);
    g.setColour(ink);g.setFont(font(valueType,true));g.drawText(getButtonText(),r,juce::Justification::centred);
    if(hasKeyboardFocus(false)){g.setColour(forest);g.drawRoundedRectangle(r.reduced(3),pill-3,1);}
}

