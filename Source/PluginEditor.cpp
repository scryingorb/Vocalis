#include "PluginEditor.h"
using namespace VocalisTheme;
namespace {
juce::Rectangle<int> gridCell(juce::Rectangle<int> area,int index,int columns,int rows,int spacing=14)
{
    const int w=(area.getWidth()-(columns-1)*spacing)/columns;
    const int h=(area.getHeight()-(rows-1)*spacing)/rows;
    return {area.getX()+(index%columns)*(w+spacing),area.getY()+(index/columns)*(h+spacing),w,h};
}
void labelledControl(juce::Rectangle<int> area,juce::Label& label,juce::Component& control)
{
    label.setBounds(area.removeFromTop(22));area.removeFromTop(3);control.setBounds(area);
}
}
VocalisAudioProcessorEditor::VocalisAudioProcessorEditor(VocalisAudioProcessor& owner)
    : AudioProcessorEditor(owner),processor(owner)
{
    setLookAndFeel(&lookAndFeel);
    for(auto* panelComponent:{&tunePanel,&voicePanel,&humanizePanel,&presetPanel,&bottomPanel})addAndMakeVisible(*panelComponent);
    const auto label=[this](juce::Label& l,const juce::String& text)
    {
        l.setText(text,juce::dontSendNotification);l.setFont(font(labelType,true));
        l.setJustificationType(juce::Justification::centred);l.setInterceptsMouseClicks(false,false);addAndMakeVisible(l);
    };
    const auto attach=[this,&label](juce::Slider& slider,juce::Label& l,const char* id,const char* caption,const char* tip,bool rotary=false)
    {
        label(l,caption);slider.setName(caption);slider.setTooltip(tip);
        if(!rotary){slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,74,26);slider.setNumDecimalPlacesToDisplay(1);}
        auto* parameter=processor.parameters.getParameter(id);
        slider.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));
        addAndMakeVisible(slider);sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters,id,slider));
    };
    label(keyLabel,"Key");label(scaleLabel,"Scale");label(presetStatus,"Custom");presetStatus.setJustificationType(juce::Justification::centredRight);
    keySelector.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
    scaleSelector.addItemList({"Major","Minor","Chromatic"},1);
    keySelector.setName("Key");scaleSelector.setName("Scale");
    keySelector.setTooltip("Song tonic. Ignored in Chromatic; A4 = 440 Hz.");scaleSelector.setTooltip("Minor is natural minor.");
    addAndMakeVisible(keySelector);addAndMakeVisible(scaleSelector);
    keyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,ParameterIDs::key,keySelector);
    scaleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,ParameterIDs::scale,scaleSelector);
    const char* tuneIDs[]={ParameterIDs::correctionAmount,ParameterIDs::retuneSpeed,ParameterIDs::hardTune,ParameterIDs::noteTransition};
    const char* tuneNames[]={"Correction","Retune","Natural / Hard","Note Transition"};
    const char* tuneTips[]={"Correction strength. 0% disables automatic tuning.","Higher values tune faster: 250 ms to 4 ms, plus analysis response.","Natural at 0%; tight instantaneous correction at 100%. Humanize and Bloom can soften it.","Higher values confirm new target notes longer before switching."};
    for(size_t i=0;i<tune.size();++i){attach(tune[i],tuneLabels[i],tuneIDs[i],tuneNames[i],tuneTips[i]);tune[i].setTextValueSuffix(" %");}
    attach(pitch,voiceLabels[0],ParameterIDs::pitch,"Pitch","Manual transposition added after automatic correction. Semitones.",true);
    attach(formant,voiceLabels[1],ParameterIDs::formantShift,"Formant","Base vocal resonance shift in semitones. Bloom adds up to +0.75 st.",true);
    attach(preserve,voiceLabels[2],ParameterIDs::formantPreserve,"Formant Preserve","100% attempts to preserve formants against combined pitch movement.",true);
    attach(bloom,voiceLabels[3],ParameterIDs::bloom,"Bloom",bloom.getTooltip().toRawUTF8(),true);
    const char* humanizeIDs[]={ParameterIDs::humanize,ParameterIDs::vibratoPreserve,ParameterIDs::pitchDrift,ParameterIDs::transitionSmoothness};
    const char* humanizeNames[]={"Humanize","Vibrato","Drift","Smoothness"};
    const char* humanizeTips[]={"Base naturalness: soften centering, retain movement and ease transitions.","Preserve more existing vibrato. Does not generate vibrato.","Retain slow measured movement, bounded to four cents; no random pitch noise.","Ease retuning around note changes. Bloom can add effective smoothing."};
    for(size_t i=0;i<humanize.size();++i)attach(humanize[i],humanizeLabels[i],humanizeIDs[i],humanizeNames[i],humanizeTips[i],true);
    for(size_t i=0;i<presetButtons.size();++i)
    {
        presetButtons[i]=std::make_unique<PresetCapsule>(PresetManager::factory()[i].name);
        presetButtons[i]->onClick=[this,i]{processor.presets.apply(i);timerCallback();};addAndMakeVisible(*presetButtons[i]);
    }
    attach(input,inputLabel,ParameterIDs::inputGain,"Input","Input gain for the processed branch; dB.");input.setTextValueSuffix(" dB");
    attach(mix,mixLabel,ParameterIDs::dryWet,"Dry/Wet","Blend time-aligned dry and processed audio.");mix.setTextValueSuffix(" %");
    attach(output,outputLabel,ParameterIDs::outputGain,"Output","Output gain after the dry/wet blend; dB.");output.setTextValueSuffix(" dB");
    addAndMakeVisible(bypass);bypass.setTooltip("Bypass processing with the existing aligned delay and crossfade.");
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,ParameterIDs::bypass,bypass);
    addAndMakeVisible(orb);addAndMakeVisible(logo);addAndMakeVisible(inputMeter);addAndMakeVisible(outputMeter);
    setResizable(true,true);setResizeLimits(1000,800,1600,1200);setSize(1120,900);
    sendLookAndFeelChange(); // Refresh text boxes created before controls joined the editor.
    timerCallback();startTimerHz(20);
}
VocalisAudioProcessorEditor::~VocalisAudioProcessorEditor(){stopTimer();setLookAndFeel(nullptr);}
void VocalisAudioProcessorEditor::timerCallback()
{
    orb.setFrequency(processor.detectedPitchHz.load(std::memory_order_relaxed),processor.parameters.getRawParameterValue(ParameterIDs::correctionAmount)->load()>0);
    inputMeter.setPeak(processor.inputPeak.exchange(0,std::memory_order_relaxed));
    outputMeter.setPeak(processor.outputPeak.exchange(0,std::memory_order_relaxed));
    const int active=processor.presets.activeIndex();
    for(size_t i=0;i<presetButtons.size();++i)presetButtons[i]->setToggleState(active==int(i),juce::dontSendNotification);
    presetStatus.setText(active<0?"Custom":PresetManager::factory()[size_t(active)].name,juce::dontSendNotification);
}
void VocalisAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat();
        juce::ColourGradient atmosphere(sky,0,0,paper,0,float(getHeight()),false);
    atmosphere.addColour(.42,glass);atmosphere.addColour(.85,mint.interpolatedWith(paper,.72f));
    g.setGradientFill(atmosphere);g.fillAll();
    g.setGradientFill(juce::ColourGradient(aqua.withAlpha(.34f),r.getRight(),0,aqua.withAlpha(0.0f),r.getWidth()*.35f,r.getHeight()*.6f,true));g.fillAll();
    g.setColour(juce::Colours::white.withAlpha(.22f));g.fillEllipse(r.getWidth()*.7f,-r.getHeight()*.25f,r.getWidth()*.5f,r.getWidth()*.5f);
}
void VocalisAudioProcessorEditor::resized()
{
    auto area=getLocalBounds().reduced(inset);
    headerBounds=area.removeFromTop(juce::jlimit(160,230,int(getHeight()*.22)));area.removeFromTop(gap);
    auto header = headerBounds;
    orb.setBounds(header.removeFromRight(headerBounds.getHeight()));
    // Reserve equal side widths so the brand stays centred in the window.
    logo.setBounds(headerBounds.reduced(headerBounds.getHeight()+gap, 0));
    bottomPanel.setBounds(area.removeFromBottom(96));area.removeFromBottom(gap);
    presetPanel.setBounds(area.removeFromBottom(134));area.removeFromBottom(gap);
    tunePanel.setBounds(gridCell(area,0,3,1));voicePanel.setBounds(gridCell(area,1,3,1));humanizePanel.setBounds(gridCell(area,2,3,1));
    auto tuning=tunePanel.contentBounds();auto selections=tuning.removeFromTop(60);tuning.removeFromTop(6);
    labelledControl(gridCell(selections,0,2,1,10),keyLabel,keySelector);labelledControl(gridCell(selections,1,2,1,10),scaleLabel,scaleSelector);
    for(size_t i=0;i<tune.size();++i)labelledControl(gridCell(tuning,int(i),1,4,5),tuneLabels[i],tune[i]);
    std::array<juce::Component*,4> voice{{&pitch,&formant,&preserve,&bloom}};
    for(size_t i=0;i<voice.size();++i)labelledControl(gridCell(voicePanel.contentBounds(),int(i),2,2,8),voiceLabels[i],*voice[i]);
    for(size_t i=0;i<humanize.size();++i)labelledControl(gridCell(humanizePanel.contentBounds(),int(i),2,2,8),humanizeLabels[i],humanize[i]);
    auto presetArea=presetPanel.contentBounds();
    auto row=presetArea.removeFromTop((presetArea.getHeight()-4)/2);presetArea.removeFromTop(4);
    for(size_t i=0;i<5;++i)presetButtons[i]->setBounds(gridCell(row,int(i),5,1,8));
    presetArea.reduce((row.getWidth()/5)/2,0);
    for(size_t i=5;i<9;++i)presetButtons[i]->setBounds(gridCell(presetArea,int(i-5),4,1,8));
    auto status=presetPanel.getBounds().reduced(inset);status.setHeight(22);status.removeFromLeft(status.getWidth()/2);presetStatus.setBounds(status);
    auto bottom=bottomPanel.contentBounds();bypass.setBounds(bottom.removeFromRight(110).reduced(0,10));bottom.removeFromRight(gap);
    std::array<juce::Slider*,3> gains{{&input,&mix,&output}};std::array<juce::Label*,3> labels{{&inputLabel,&mixLabel,&outputLabel}};
    for(int i=0;i<3;++i)
    {
        auto cell=gridCell(bottom,i,3,1,18);
        if(i==0){inputMeter.setBounds(cell.removeFromLeft(20));cell.removeFromLeft(8);}
        if(i==2){outputMeter.setBounds(cell.removeFromRight(20));cell.removeFromRight(8);}
        labelledControl(cell,*labels[size_t(i)],*gains[size_t(i)]);
    }
}

