#include "PitchOrb.h"
PitchOrb::PitchOrb(){setTitle("Pitch Orb");setDescription("Detected input note and cents from equal temperament, A4 = 440 Hz.");setInterceptsMouseClicks(false,false);}
void PitchOrb::setFrequency(float hz,bool enabled)
{
    juce::String nextNote="--",nextCents="--";
    if(enabled&&std::isfinite(hz)&&hz>0)
    {
        const double midi=69+12*std::log2(hz/440.0);
        const int nearest=juce::roundToInt(midi);
        static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        nextNote=juce::String(names[(nearest%12+12)%12])+juce::String(nearest/12-1);
        const int offset=juce::roundToInt((midi-nearest)*100);
        nextCents=(offset<0?"-":"+")+juce::String(std::abs(offset)).paddedLeft('0',2);
    }
    if(note!=nextNote||cents!=nextCents){note=nextNote;cents=nextCents;repaint();}
}
void PitchOrb::paint(juce::Graphics& g)
{
    auto area=getLocalBounds().toFloat().reduced(12);
    const float diameter=juce::jmin(area.getWidth(),area.getHeight());
    auto circle=juce::Rectangle<float>(diameter,diameter).withCentre(area.getCentre());
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.beginTransparencyLayer(note=="--"?.55f:1.f);
        VocalisTheme::bubble(g,circle,note!="--");
        g.endTransparencyLayer();
    }
    g.setColour(VocalisTheme::forest);g.setFont(VocalisTheme::font(diameter*.064f,true));
    g.drawText("INPUT PITCH",circle.withY(circle.getY()+diameter*.15f).withHeight(diameter*.12f),juce::Justification::centred);
    g.setColour(VocalisTheme::ink);g.setFont(VocalisTheme::font(diameter*.30f,true));
    g.drawText(note,circle.withY(circle.getY()+diameter*.29f).withHeight(diameter*.35f),juce::Justification::centred);
    g.setFont(VocalisTheme::font(diameter*.12f));g.drawText(cents,circle.withY(circle.getY()+diameter*.64f).withHeight(diameter*.17f),juce::Justification::centred);
    g.setColour(VocalisTheme::muted);g.setFont(VocalisTheme::font(diameter*.06f));g.drawText("CENTS",circle.withY(circle.getY()+diameter*.81f).withHeight(diameter*.09f),juce::Justification::centred);
}


