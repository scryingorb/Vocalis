#include "BubbleKnob.h"
BubbleKnob::BubbleKnob(juce::String unit,bool signedValue,int decimals)
    : units(std::move(unit)),signedText(signedValue),precision(decimals)
{
    setSliderStyle(RotaryHorizontalVerticalDrag);setTextBoxStyle(TextBoxBelow,false,110,25);
    setDoubleClickReturnValue(true,0);
}
juce::String BubbleKnob::getTextFromValue(double value)
{
    return (signedText&&value>0?"+":"")+juce::String(value,precision)+" "+units;
}
double BubbleKnob::getValueFromText(const juce::String& text){return text.trimStart().getDoubleValue();}
