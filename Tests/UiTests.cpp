#include "../Source/PluginProcessor.h"
#include "../Source/ui/PitchOrb.h"
#include "../Source/ui/BubbleButton.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

void processWithoutAllocations(juce::AudioProcessor&,juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void writeSnapshot(juce::Component& editor,const juce::String& name)
{
    auto snapshot=editor.createComponentSnapshot(editor.getLocalBounds());
    juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile(name));stream.setPosition(0);stream.truncate();
    juce::PNGImageFormat png;check(png.writeImageToStream(snapshot,stream),"UI image export failed");
}
void layoutTests()
{
    // Deterministic visual review of interaction states without moving the OS mouse.
    juce::Image states(juce::Image::RGB,800,64,true);
    {
    juce::Graphics stateGraphics(states);stateGraphics.fillAll(VocalisTheme::glass);
    for(int state=0;state<4;++state)
    {
        const char* names[]={"Normal","Hover","Pressed","Selected"};
        BubbleButton button(names[state]);button.setSize(190,44);
        button.setToggleState(state==3,juce::dontSendNotification);
        juce::Graphics::ScopedSaveState saved(stateGraphics);
        stateGraphics.setOrigin(state*200+5,10);
        button.paintButton(stateGraphics,state==1,state==2);
    }
    } // Flush the graphics context before encoding the image.
    {
        juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile("Vocalis-button-states.png"));
        stream.setPosition(0);stream.truncate();
        juce::PNGImageFormat png;check(png.writeImageToStream(states,stream),"Button-state image export failed");
    }
    PitchOrb orb;orb.setFrequency(220*std::pow(2.f,-.07f/12),true);
    check(orb.noteText()=="A3"&&orb.centsText()=="-07","Pitch Orb note/cents formatting");
    orb.setFrequency(440,true);check(orb.noteText()=="A4"&&orb.centsText()=="+00","Pitch Orb zero cents");
    orb.setFrequency(440,false);check(orb.noteText()=="--"&&orb.centsText()=="--","Pitch Orb inactive display");
    VocalisAudioProcessor p;p.presets.apply(3);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor->isResizable(),"Editor not resizable");
    for(auto size:{juce::Point<int>{900,700},{1000,750},{1000,800},{1120,900},{1600,1200},{1450,850}})
    {
        editor->setSize(size.x,size.y);
        std::vector<juce::Component*> interactive;
        for(auto* child:editor->getChildren())
        {
            if(!child->isVisible())continue;
            check(editor->getLocalBounds().contains(child->getBounds()),"Resized child outside editor");
            if(dynamic_cast<juce::Slider*>(child)||dynamic_cast<juce::ComboBox*>(child)||dynamic_cast<juce::Button*>(child))
            {
                check(child->getWidth()>=60&&child->getHeight()>=26,"Control too small after resize");interactive.push_back(child);
            }
            if(auto* slider=dynamic_cast<juce::Slider*>(child))
            {
                bool foundValueField=false;
                for(auto* sliderChild:slider->getChildren())
                    if(auto* valueField=dynamic_cast<juce::Label*>(sliderChild))
                    {
                        foundValueField=true;
                        check(valueField->isVisible()&&valueField->isEditable(),"Slider value field is not editable");
                        check(slider->getLocalBounds().contains(valueField->getBounds()),"Slider value field outside control");
                        check(valueField->getWidth()>=slider->getTextBoxWidth()
                              &&valueField->getHeight()>=slider->getTextBoxHeight(),"Slider value field compressed after resize");
                    }
                check(foundValueField,"Slider value field missing");
            }
            if(auto* pitchOrb=dynamic_cast<PitchOrb*>(child))pitchOrb->setFrequency(220*std::pow(2.f,-.07f/12),true);
        }
        for(size_t i=0;i<interactive.size();++i)for(size_t j=i+1;j<interactive.size();++j)
            check(!interactive[i]->getBounds().intersects(interactive[j]->getBounds()),"Interactive controls overlap");
        check(interactive.size()==27,"Missing attached controls or preset buttons"); // 18 parameters + 9 presets
        std::vector<int> focusOrders;
        for(auto* control:interactive)
        {
            check(control->getWantsKeyboardFocus(),"Control missing keyboard access");
            focusOrders.push_back(control->getExplicitFocusOrder());
        }
        std::sort(focusOrders.begin(),focusOrders.end());
        for(size_t i=0;i<focusOrders.size();++i)
            check(focusOrders[i]==int(i)+1,"Keyboard order is missing or duplicated");
        writeSnapshot(*editor,"Vocalis-ui-"+juce::String(size.x)+"x"+juce::String(size.y)+".png");
    }
}
void unchangedAudioTests()
{
    // Compare against the actual Phase 7 DLL copied before any Phase 8 changes.
    auto baseline=juce::File::getCurrentWorkingDirectory().getChildFile("phase8-baseline/Vocalis.vst3");
    if(!baseline.exists()){std::cout<<"SKIP: historical Phase 7 binary comparison (baseline bundle absent)\n";return;}
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,baseline.getFullPathName());check(descriptions.size()==1,"Baseline VST3 discovery");
    juce::String error;auto old=format.createInstanceFromDescription(*descriptions[0],48000,256,error);
    check(old!=nullptr,"Baseline VST3 load failed");
    check(old->getParameters().size()==18,"Baseline parameter layout changed");
    for(double rate:{44100.,48000.,88200.,96000.})for(size_t preset=0;preset<PresetManager::factoryCount;++preset)
    {
        VocalisAudioProcessor current;current.presets.apply(preset);
        for(int i=0;i<18;++i)old->getParameters()[i]->setValueNotifyingHost(current.getParameters()[i]->getValue());
        current.prepareToPlay(rate,511);old->prepareToPlay(rate,511);old->reset();
        check(current.getLatencySamples()==old->getLatencySamples(),"UI change altered host latency");
        std::unique_ptr<juce::AudioProcessorEditor> editor(current.createEditor());
        juce::AudioBuffer<float> a(2,511),b(2,511);juce::MidiBuffer midi;
        for(int block=0;block<100;++block)
        {
            for(int ch=0;ch<2;++ch)for(int i=0;i<511;++i)
            {
                const double phase=6.283185307179586*(block*511+i)*(ch==0?225:330)/rate;
                const float x=float(.08*std::sin(phase)+.025*std::sin(phase*2));a.setSample(ch,i,x);b.setSample(ch,i,x);
            }
            processWithoutAllocations(*old,a,midi,false);processWithoutAllocations(current,b,midi,false);
            for(int ch=0;ch<2;++ch)for(int i=0;i<511;++i)
                check(a.getSample(ch,i)==b.getSample(ch,i),"Redesign changed audio versus Phase 7 binary");
            // Consume meter data as the UI does; it must not change future audio.
            check(current.inputPeak.exchange(0)>=0&&current.outputPeak.exchange(0)>=0,"Invalid meter telemetry");
        }
        old->releaseResources();
    }
    std::cout<<"PASS: sample-exact audio versus Phase 7 VST3 across all presets/rates\n";
}
}
void runUiTests()
{
    layoutTests();unchangedAudioTests();
    std::cout<<"PASS: resize layouts and Pitch Orb\n";
}

