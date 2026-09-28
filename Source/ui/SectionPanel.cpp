#include "SectionPanel.h"
SectionPanel::SectionPanel(juce::String title) : heading(std::move(title)) {setInterceptsMouseClicks(false,false);}
void SectionPanel::paint(juce::Graphics& g)
{
    VocalisTheme::panel(g,getLocalBounds().toFloat().reduced(2,4));
    g.setColour(VocalisTheme::forest);g.setFont(VocalisTheme::font(VocalisTheme::sectionType,true).withExtraKerningFactor(.06f));
    g.drawText(heading,getLocalBounds().reduced(VocalisTheme::inset).removeFromTop(22),juce::Justification::centredLeft);
}
juce::Rectangle<int> SectionPanel::contentBounds() const
{
    auto r=getBounds().reduced(VocalisTheme::inset);
    if(heading.isNotEmpty())r.removeFromTop(28);
    return r;
}

