#include "VocalisLookAndFeel.h"
#include "VocalisTheme.h"
using namespace VocalisTheme;
VocalisLookAndFeel::VocalisLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,paper);
    setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId,forest);
    setColour(juce::Label::textColourId,ink);
    setColour(juce::ComboBox::textColourId,ink);setColour(juce::ComboBox::backgroundColourId,glass);
    setColour(juce::PopupMenu::backgroundColourId,paper);setColour(juce::PopupMenu::textColourId,ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,mint);setColour(juce::PopupMenu::highlightedTextColourId,ink);
    setColour(juce::TooltipWindow::backgroundColourId,paper);setColour(juce::TooltipWindow::textColourId,ink);
}
void VocalisLookAndFeel::drawLabel(juce::Graphics& g,juce::Label& label)
{
    if(dynamic_cast<juce::Slider*>(label.getParentComponent())!=nullptr)
    {
        const auto r=label.getLocalBounds().toFloat().reduced(.5f);
        g.setGradientFill(juce::ColourGradient(dark,r.getX(),r.getY(),ink,r.getRight(),r.getBottom(),false));
        g.fillRoundedRectangle(r,8);
        g.setColour(cyan.withAlpha(.45f));g.drawRoundedRectangle(r,8,1);
    }
    juce::LookAndFeel_V4::drawLabel(g,label);
}
void VocalisLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& slider)
{
    auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(slider.getName()=="Pitch"||slider.getName()=="Formant"?7.f:12.f);
    const float size=juce::jmin(bounds.getWidth(),bounds.getHeight());
    auto circle=juce::Rectangle<float>(size,size).withCentre(bounds.getCentre());
    const bool bloom=slider.getName()=="Bloom";
    bubble(g,circle,bloom||slider.isMouseOverOrDragging());
    const auto centre=circle.getCentre();const float radius=size*.5f;
    juce::Path track,arc;
    track.addCentredArc(centre.x,centre.y,radius+4,radius+4,0,start,end,true);
    g.setColour(forest.withAlpha(.13f));g.strokePath(track,juce::PathStrokeType(3));
    arc.addCentredArc(centre.x,centre.y,radius+4,radius+4,0,start,start+position*(end-start),true);
    g.setColour(forest);g.strokePath(arc,juce::PathStrokeType(3,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    juce::Path pointer;pointer.addRoundedRectangle(-1.8f,-radius*.77f,3.6f,radius*.32f,1.8f);
    g.setColour(dark);g.fillPath(pointer,juce::AffineTransform::rotation(start+position*(end-start)).translated(centre.x,centre.y));
}
void VocalisLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float,float,juce::Slider::SliderStyle,juce::Slider&)
{
    const float cy=float(y)+float(height)*.5f;
    juce::Rectangle<float> track(float(x),cy-3,float(width),6);
    g.setColour(forest.withAlpha(.16f));g.fillRoundedRectangle(track,3);
    g.setGradientFill(juce::ColourGradient(forest,float(x),cy,cyan,position,cy,false));
    g.fillRoundedRectangle(track.withWidth(juce::jmax(0.0f,position-float(x))),3);
    bubble(g,{position-7,cy-7,14,14});
}
void VocalisLookAndFeel::drawComboBox(juce::Graphics& g,int width,int height,bool down,int,int,int,int,juce::ComboBox& box)
{
    auto r=juce::Rectangle<float>(0,0,float(width),float(height)).reduced(1);
    capsule(g,r,box.isPopupActive(),box.isMouseOver(),down);
    g.setColour(box.hasKeyboardFocus(true)?forest:forest.withAlpha(.3f));g.drawRoundedRectangle(r,10,1);
    juce::Path arrow;const float cx=float(width)-16,cy=float(height)*.5f;
    arrow.startNewSubPath(cx-4,cy-2);arrow.lineTo(cx,cy+2);arrow.lineTo(cx+4,cy-2);
    g.setColour(ink);g.strokePath(arrow,juce::PathStrokeType(1.5f));
}

juce::Font VocalisLookAndFeel::getComboBoxFont(juce::ComboBox&) { return font(valueType); }
juce::Font VocalisLookAndFeel::getPopupMenuFont() { return font(valueType); }

