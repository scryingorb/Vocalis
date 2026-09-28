#pragma once
#include "VocalisLookAndFeel.h"

namespace VocalisTheme
{
using Palette = VocalisLookAndFeel::Palette;
inline const auto lime=Palette::mint, green=Palette::green, mint=Palette::mint,
    glass=Palette::glass, aqua=Palette::aqua, forest=Palette::deepAqua,
    paper=Palette::pearl, ink=Palette::ink, muted=Palette::secondary,
    sky=Palette::sky, cyan=Palette::cyan, dark=Palette::dark, white=Palette::white;
inline constexpr float corner=24, pill=16, sectionType=13, labelType=13, valueType=14, brandType=36;
inline constexpr int gap=14, inset=18;
inline juce::Font font(float size,bool bold=false){return juce::Font(juce::FontOptions(size,bold?juce::Font::bold:juce::Font::plain));}
inline void reflection(juce::Graphics& g,juce::Rectangle<float> r,float radius,float strength)
{
    g.setGradientFill(juce::ColourGradient(white.withAlpha(strength),r.getX(),r.getY(),white.withAlpha(0.f),r.getX(),r.getBottom(),false));
    g.fillRoundedRectangle(r,radius);
}
inline void panel(juce::Graphics& g,juce::Rectangle<float> r)
{
    g.setColour(dark.withAlpha(.08f));g.fillRoundedRectangle(r.translated(0,4),corner);
    juce::ColourGradient gradient(paper.withAlpha(.88f),r.getX(),r.getY(),cyan.withAlpha(.20f),r.getRight(),r.getBottom(),false);
    gradient.addColour(.55,glass.withAlpha(.64f));gradient.addColour(.9,mint.withAlpha(.24f));
    g.setGradientFill(gradient);g.fillRoundedRectangle(r,corner);
    g.setColour(forest.withAlpha(.28f));g.drawRoundedRectangle(r,corner,1);
    g.setColour(white.withAlpha(.85f));g.drawRoundedRectangle(r.reduced(1.5f),corner-1,1);
    reflection(g,r.reduced(8).withHeight(15),9,.65f);
}
inline void capsule(juce::Graphics& g,juce::Rectangle<float> r,bool active,bool hover,bool down)
{
    const float radius=r.getHeight()*.5f;
    if(active||hover){g.setColour(cyan.withAlpha(active?.32f:.20f));g.fillRoundedRectangle(r.expanded(2),radius+2);}
    g.setColour(dark.withAlpha(.10f));g.fillRoundedRectangle(r.translated(0,2),radius);
    juce::ColourGradient fill(down?aqua:paper,r.getX(),r.getY(),active?aqua:sky.withAlpha(.6f),r.getX(),r.getBottom(),false);
    fill.addColour(.48,active?mint:(hover?glass:cyan.withAlpha(.28f)));
    g.setGradientFill(fill);g.fillRoundedRectangle(r,radius);
    g.setColour(forest.withAlpha(active?.8f:.35f));g.drawRoundedRectangle(r,radius,active?1.6f:1.f);
    reflection(g,r.reduced(3).withHeight(r.getHeight()*(down?.22f:.46f)),radius,hover?.95f:.7f);
    g.setColour(white.withAlpha(.8f));g.drawRoundedRectangle(r.reduced(2),radius-2,.8f);
}
inline void bubble(juce::Graphics& g,juce::Rectangle<float> r,bool glowing=false)
{
    if(glowing){g.setColour(cyan.withAlpha(.20f));g.fillEllipse(r.expanded(7));g.setColour(mint.withAlpha(.24f));g.fillEllipse(r.expanded(3));}
    g.setColour(dark.withAlpha(.12f));g.fillEllipse(r.translated(0,4));
    juce::ColourGradient body(paper,r.getX()+r.getWidth()*.28f,r.getY()+r.getHeight()*.20f,forest,r.getRight(),r.getBottom(),true);
    body.addColour(.35,cyan);body.addColour(.7,aqua);body.addColour(.88,sky);
    g.setGradientFill(body);g.fillEllipse(r);
    auto inner=r.reduced(r.getWidth()*.13f);
    g.setGradientFill(juce::ColourGradient(mint.withAlpha(glowing?.9f:.65f),inner.getRight(),inner.getBottom(),mint.withAlpha(0.f),inner.getX(),inner.getY(),true));g.fillEllipse(inner);
    g.setGradientFill(juce::ColourGradient(white.withAlpha(.95f),r.getX(),r.getY(),white.withAlpha(0.f),r.getCentreX(),r.getCentreY(),false));
    g.fillEllipse(r.reduced(r.getWidth()*.10f).withHeight(r.getHeight()*.42f));
    g.setColour(forest.withAlpha(.75f));g.drawEllipse(r,1.2f);
    g.setColour(white.withAlpha(.90f));g.drawEllipse(r.reduced(2),1.5f);
    g.setColour(cyan.withAlpha(.8f));g.drawEllipse(r.reduced(5),.8f);
}
}
