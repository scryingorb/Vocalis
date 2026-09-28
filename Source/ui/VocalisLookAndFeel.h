#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class VocalisLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    struct Palette
    {
        inline static const juce::Colour deepAqua{0xff148fa3}, aqua{0xff32c9d6},
            cyan{0xff5fe3f2}, sky{0xff8fdeff}, mint{0xffa8f5d0}, green{0xff70d891},
            glass{0xffd9faff}, pearl{0xfff7ffff}, ink{0xff145867}, dark{0xff0b4450},
            secondary{0xff326c7a}, white{0xffffffff}, warning{0xffb86516};
    };
    VocalisLookAndFeel();
    void drawLabel(juce::Graphics&,juce::Label&) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

