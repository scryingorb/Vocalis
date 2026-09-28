#include "../Source/PluginProcessor.h"
#include "../Source/dsp/BloomMapping.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void processWithoutAllocations(juce::AudioProcessor&,juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
namespace {
constexpr double tau=6.283185307179586;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void set(VocalisAudioProcessor& p,const char* id,float value){auto* a=p.parameters.getParameter(id);a->setValueNotifyingHost(a->convertTo0to1(value));}
void stateTests()
{
    VocalisAudioProcessor p;
    check(p.parameters.getRawParameterValue(ParameterIDs::bloom)->load()==0,"Bloom default");
    check(!p.presets.apply(999),"Invalid factory index accepted");
    set(p,ParameterIDs::key,9);set(p,ParameterIDs::scale,1);
    for(size_t preset=0;preset<PresetManager::factoryCount;++preset)
    {
        check(p.presets.apply(preset),"Factory recall failed");
        check(p.presets.activeIndex()==int(preset),"Active factory detection failed");
        check(p.parameters.getRawParameterValue(ParameterIDs::key)->load()==9&&p.parameters.getRawParameterValue(ParameterIDs::scale)->load()==1,"Preset changed musical key/scale");
        juce::MemoryBlock state;p.getStateInformation(state);VocalisAudioProcessor restored;
        restored.setStateInformation(state.getData(),int(state.getSize()));
        check(restored.presets.activeIndex()==int(preset),"Factory glow did not restore from state");
        for(auto id:PresetManager::parameterIDs())check(std::abs(p.parameters.getRawParameterValue(id)->load()-restored.parameters.getRawParameterValue(id)->load())<.001,"Factory state mismatch");
        set(restored,ParameterIDs::pitch,1.25f);check(restored.presets.activeIndex()==-1,"Manual edit did not become Custom");
        set(restored,ParameterIDs::pitch,PresetManager::factory()[preset].values[4]);check(restored.presets.activeIndex()==int(preset),"Matching factory did not regain glow");
    }
    auto legacy=p.parameters.copyState();for(int i=legacy.getNumChildren()-1;i>=0;--i)if(legacy.getChild(i).getProperty("id").toString()==ParameterIDs::bloom)legacy.removeChild(i,nullptr);
    set(p,ParameterIDs::bloom,100);juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),data);
    p.setStateInformation(data.getData(),int(data.getSize()));check(p.parameters.getRawParameterValue(ParameterIDs::bloom)->load()==0,"Legacy state retained Bloom");
}
std::vector<float> source(double rate,double hz)
{
    std::vector<float> result(size_t(rate*1.5),0);
    for(int h=1;h<=36;++h)
    {
        const double f=h*hz;
        const double gain=.07/h+.035*std::exp(-std::pow((f-700)/200,2))+.02*std::exp(-std::pow((f-2100)/300,2));
        for(size_t i=0;i<result.size();++i)result[i]+=float(gain*std::sin(tau*f*i/rate));
    }
    return result;
}
std::vector<float> render(double rate,int channels,size_t preset,const std::vector<float>& input,bool variable=true)
{
    VocalisAudioProcessor p;p.presets.apply(preset);p.setPlayConfigDetails(channels,channels,rate,64);p.prepareToPlay(rate,64);
    check(p.getLatencySamples()==int(rate*.048)+64,"Preset changed reported latency");
    std::vector<float> out(input.size());juce::AudioBuffer<float> buffer(channels,1024);juce::MidiBuffer midi;int block=0;
    const int sizes[]={1,17,64,127,511,1024};
    for(int pos=0;pos<int(input.size());)
    {
        int n=std::min(variable?sizes[block++%6]:64,int(input.size())-pos);buffer.setSize(channels,n,false,false,true);
        for(int ch=0;ch<channels;++ch)std::copy_n(input.data()+pos,n,buffer.getWritePointer(ch));
        processWithoutAllocations(p,buffer,midi,false);
        for(int i=0;i<n;++i){float x=buffer.getSample(0,i);check(std::isfinite(x)&&std::abs(x)<2,"Preset nonfinite/runaway output");out[size_t(pos+i)]=x;if(channels==2)check(x==buffer.getSample(1,i),"Preset stereo clock mismatch");}
        pos+=n;
    }
    return out;
}
void automationTests()
{
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})
    {
        VocalisAudioProcessor p;p.setPlayConfigDetails(channels,channels,rate,64);p.prepareToPlay(rate,64);
        juce::AudioBuffer<float> buffer(channels,127);juce::MidiBuffer midi;double phase=0;float previous=0;double step=0;
        for(int block=0;block<900;++block)
        {
            if(block%100==0)p.presets.apply(size_t(block/100));
            if(block%13==0)set(p,ParameterIDs::bloom,float((block*7)%101));
            for(int i=0;i<127;++i){float x=float(.04*std::sin(phase)+.01*std::sin(2*phase));phase+=tau*180/rate;for(int ch=0;ch<channels;++ch)buffer.setSample(ch,i,x);}
            processWithoutAllocations(p,buffer,midi,false);
            for(int i=0;i<127;++i){float x=buffer.getSample(0,i);check(std::isfinite(x)&&std::abs(x)<1,"Preset switching instability");step=std::max(step,double(std::abs(x-previous)));previous=x;}
        }
        check(step<.04,"Preset switching discontinuity");
    }
}
}
void runPresetTests()
{
    stateTests();automationTests();
    auto zero=BloomMapping::apply(0,{20,30,40,50},-1);
    check(zero.humanizer.amount==20&&zero.humanizer.vibrato==30&&zero.humanizer.drift==40&&zero.humanizer.smoothness==50&&zero.formant==-1,"Bloom zero not identity");
    auto full=BloomMapping::apply(100,{20,30,40,50},-1);
    check(full.humanizer.amount==68&&full.humanizer.vibrato==65&&full.humanizer.drift==40&&full.humanizer.smoothness==77.5f&&full.formant==-.25f,"Bloom mapping changed");
    check(BloomMapping::apply(100,{100,100,100,100},6).formant==6,"Bloom exceeded formant bound");
    std::ofstream csv("preset-measurements.csv");csv<<"rate,channels,preset,input_hz,gain_db,peak\n";
    double largestGain=-100,smallestGain=100,maxPeak=0;
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})for(double hz:{120.,220.})
    {
        auto input=source(rate,hz);
        for(size_t preset=0;preset<PresetManager::factoryCount;++preset)
        {
            auto output=render(rate,channels,preset,input);
            if(hz==120&&rate==48000)check(output==render(rate,channels,preset,input,false),"Preset depends on buffer partition");
            double a=0,b=0,peak=0;
            for(size_t i=size_t(rate*.5);i<output.size();++i){a+=input[i]*input[i];b+=output[i]*output[i];peak=std::max(peak,double(std::abs(output[i])));}
            double gain=10*std::log10(b/a);largestGain=std::max(largestGain,gain);smallestGain=std::min(smallestGain,gain);maxPeak=std::max(maxPeak,peak);
            csv<<rate<<','<<channels<<','<<PresetManager::factory()[preset].name<<','<<hz<<','<<gain<<','<<peak<<'\n';csv.flush();
            if(gain>6||gain< -12||peak>1)std::cout<<"Preset level "<<PresetManager::factory()[preset].name<<" at "<<rate<<" / "<<hz<<": "<<gain<<" dB, peak="<<peak<<std::endl;
            check(gain<=6&&gain>=-12&&peak<1,"Extreme factory preset gain change");
        }
    }
    std::cout<<"Factory measured gain range "<<smallestGain<<" to "<<largestGain<<" dB, max peak "<<maxPeak<<'\n';
    std::cout<<"PASS: Bloom mapping, nine presets, state, manual edits, recall, levels, variable buffers, switching and allocation safety\n";
}

void runPresetEditorTest(juce::AudioProcessorEditor& editor, VocalisAudioProcessor& processor)
{
    std::array<juce::TextButton*,PresetManager::factoryCount> capsules{};
    for(auto* child:editor.getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child))
        for(size_t i=0;i<capsules.size();++i)if(button->getButtonText()==PresetManager::factory()[i].name)capsules[i]=button;
    for(size_t i=0;i<capsules.size();++i)
    {
        check(capsules[i]!=nullptr,"Factory capsule missing");
        capsules[i]->onClick();
        check(processor.presets.activeIndex()==int(i),"Capsule did not apply preset");
        for(size_t j=0;j<capsules.size();++j)check(capsules[j]!=nullptr&&capsules[j]->getToggleState()==(i==j),"Active preset glow incorrect");
    }
    capsules[3]->onClick(); // Snapshot an active artistic character.
}
