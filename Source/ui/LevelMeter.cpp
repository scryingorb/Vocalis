#include "LevelMeter.h"
LevelMeter::LevelMeter(juce::String title) : caption(std::move(title)) {setInterceptsMouseClicks(false,false);setName(caption+" peak meter");}
void LevelMeter::setPeak(float peak)
{
    const float next=juce::jmax(juce::jlimit(0.0f,2.0f,peak),level*.80f);
    const float quantized=next<.0001f?0:next;
    if(std::abs(level-quantized)>.0001f||(quantized==0&&level!=0)){level=quantized;repaint();}
}
void LevelMeter::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat();
    g.setColour(VocalisTheme::muted);g.setFont(VocalisTheme::font(10));g.drawText(caption,r.removeFromTop(15),juce::Justification::centred);
    r=r.reduced(3,2);g.setColour(VocalisTheme::forest.withAlpha(.12f));g.fillRoundedRectangle(r,4);
    const float normalized=juce::jlimit(0.0f,1.0f,((level>0?20.0f*std::log10(level):-60.0f)+60)/60);
    auto fill=r.withTrimmedTop(r.getHeight()*(1-normalized)); // Keep the glass tube for its reflection.
    g.setGradientFill(juce::ColourGradient(level>=1?VocalisTheme::Palette::warning:VocalisTheme::mint,0,fill.getY(),VocalisTheme::forest,0,fill.getBottom(),false));g.fillRoundedRectangle(fill,4);
    VocalisTheme::reflection(g,r.reduced(1).withWidth(3),2,.75f);
    g.setColour(VocalisTheme::forest.withAlpha(.3f));g.drawRoundedRectangle(r,4,.8f);
}


