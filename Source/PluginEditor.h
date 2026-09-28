#pragma once
#include "PluginProcessor.h"
#include "ui/VocalisLookAndFeel.h"
#include "ui/BubbleKnob.h"
#include "ui/BubbleButton.h"
#include "ui/PresetCapsule.h"
#include "ui/PitchOrb.h"
#include "ui/SectionPanel.h"
#include "ui/LevelMeter.h"
#include "ui/BloomControl.h"
#include "ui/VocalisLogo.h"

class VocalisAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalisAudioProcessorEditor(VocalisAudioProcessor&);
    ~VocalisAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    VocalisAudioProcessor& processor;
    VocalisLookAndFeel lookAndFeel;
    SectionPanel tunePanel{"TUNE"}, voicePanel{"VOICE"}, humanizePanel{"HUMANIZE"}, presetPanel{"VOCALIS PRESETS"}, bottomPanel;
    PitchOrb orb;
    VocalisLogo logo;
    juce::Label keyLabel, scaleLabel, presetStatus;
    juce::ComboBox keySelector, scaleSelector;
    std::array<juce::Slider,4> tune;
    std::array<juce::Label,4> tuneLabels;
    BubbleKnob pitch, formant, preserve{"%",false,1};
    BloomControl bloom;
    std::array<juce::Label,4> voiceLabels;
    std::array<BubbleKnob,4> humanize{{BubbleKnob{"%",false,1},BubbleKnob{"%",false,1},BubbleKnob{"%",false,1},BubbleKnob{"%",false,1}}};
    std::array<juce::Label,4> humanizeLabels;
    std::array<std::unique_ptr<PresetCapsule>,PresetManager::factoryCount> presetButtons;
    juce::Slider input,mix,output;
    juce::Label inputLabel,mixLabel,outputLabel;
    LevelMeter inputMeter{"IN"},outputMeter{"OUT"};
    BubbleButton bypass{"Bypass",true};
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment,scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::TooltipWindow tooltip{this,650};
    juce::Rectangle<int> headerBounds;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalisAudioProcessorEditor)
};
