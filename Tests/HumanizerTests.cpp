#include "../Source/PluginProcessor.h"
#include "../Source/dsp/Humanizer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void processWithoutAllocations(juce::AudioProcessor&, juce::AudioBuffer<float>&, juce::MidiBuffer&, bool);
void processDspWithoutAllocations(PitchShifter&, const float* const*, float* const*, int);
namespace {
constexpr double tau = 6.283185307179586;
const char* ids[] = {ParameterIDs::humanize, ParameterIDs::vibratoPreserve, ParameterIDs::pitchDrift, ParameterIDs::transitionSmoothness};
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(VocalisAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void mappingTests()
{
    Humanizer h; h.prepare(.005);
    for(int i=0;i<100;++i) {h.advance();h.observe(57.25f,true,i==0);}
    check(h.hardness(1)==1 && h.shapeCorrection(-.25f)==-.25f && h.retuneSeconds(.004)==.004 && h.transitionSeconds()==0,"Zero Humanizer changed legacy correction");
    h.setControls({100,0,0,0});
    h.advance();
    check(h.hardness(1)>.9f,"Humanize automation did not ramp");
    for(int i=0;i<1000;++i) {h.advance();h.observe(57.25f,true,false);}
    check(h.hardness(1)<.31f && h.shapeCorrection(-.25f)>-.18f,"Humanize macro did not soften tuning");
    check(h.retuneSeconds(.004)>.02 && h.transitionSeconds()>.03,"Humanize macro did not ease transitions");
    h.setControls({0,100,0,0});h.reset();
    check(h.hardness(1)==0,"Vibrato refinement failed");
    h.setControls({0,0,0,100});h.reset();
    check(h.retuneSeconds(.004)>.09 && h.transitionSeconds()==.060,"Smoothness refinement failed");
    h.setControls({0,0,100,0});h.reset();
    double maximum=0;
    for(int i=0;i<4000;++i)
    {
        h.advance();h.observe(float(57+.2*std::sin(tau*.3*i*.005)),true,i==0);
        double drift=h.shapeCorrection(0);maximum=std::max(maximum,std::abs(drift));
        check(std::abs(drift)<=.040001,"Drift exceeded four cents");
    }
    check(maximum>.02,"Drift did not retain measured movement");
    h.observe(57,true,true);
    for(int i=0;i<2000;++i) {h.advance();h.observe(57,true,false);}
    check(h.shapeCorrection(0)==0,"Drift was generated on a steady note");
    h.observe(0,false,false);check(h.shapeCorrection(0)==0,"Unvoiced material retained drift");
    h.setControls({100,0,0,0});h.reset();h.observe(57.25f,true,true);
    float attack=h.shapeCorrection(-.25f);
    for(int i=0;i<300;++i) {h.advance();h.observe(57.25f,true,false);}
    check(h.shapeCorrection(-.25f)>attack+.02f,"Humanize strength did not relax over sustain");
}
void stateTests()
{
    VocalisAudioProcessor p;
    for(auto id:ids) check(p.parameters.getRawParameterValue(id)->load()==0,"Humanizer default must retain old sessions");
    const float values[]={81.2f,63.4f,27.6f,49.8f};
    for(int i=0;i<4;++i)set(p,ids[i],values[i]);
    juce::MemoryBlock data;p.getStateInformation(data);VocalisAudioProcessor restored;
    restored.setStateInformation(data.getData(),int(data.getSize()));
    for(int i=0;i<4;++i)check(std::abs(restored.parameters.getRawParameterValue(ids[i])->load()-values[i])<.001,"Humanizer state round-trip failed");
    auto legacy=p.parameters.copyState();
    for(int i=legacy.getNumChildren()-1;i>=0;--i)for(auto id:ids)if(legacy.getChild(i).getProperty("id").toString()==id){legacy.removeChild(i,nullptr);break;}
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),data);restored.setStateInformation(data.getData(),int(data.getSize()));
    for(auto id:ids)check(restored.parameters.getRawParameterValue(id)->load()==0,"Legacy restore retained humanization");
}
std::vector<float> render(double rate,int channels,Humanizer::Controls controls,bool vibrato,bool variable,
                          PitchShifter::Mode mode=PitchShifter::Mode::live)
{
    PitchShifter dsp;dsp.setHumanizeControls(controls);dsp.prepare(rate,channels,0,mode,0,0);
    dsp.setCorrectionControls({0,2,100,100,0,100});
    check(dsp.latencySamples()==int(rate*(mode==PitchShifter::Mode::live?.048:.12))+64,"Humanizer changed latency");
    std::vector<float> input(size_t(rate*3),0),left(input.size()),right(input.size());double phase=0;
    for(size_t i=0;i<input.size();++i)
    {
        double note=57.25+(vibrato?.22*std::sin(tau*5*i/rate):0);
        phase+=tau*440*std::pow(2.,(note-69)/12)/rate;
        input[i]=float(.07*std::sin(phase));
    }
    const int sizes[]={1,17,64,127,511,1024};int block=0;
    for(int pos=0;pos<int(input.size());)
    {
        int count=std::min(variable?sizes[block++%6]:64,int(input.size())-pos);
        const float* in[]={input.data()+pos,input.data()+pos};float* out[]={left.data()+pos,right.data()+pos};
        processDspWithoutAllocations(dsp,in,out,count);pos+=count;
    }
    for(size_t i=0;i<left.size();++i)
    {
        check(std::isfinite(left[i])&&std::abs(left[i])<1,"Humanizer unstable output");
        if(channels==2 && mode==PitchShifter::Mode::live)check(left[i]==right[i],"Humanizer stereo desync");
    }
    return left;
}
struct Metrics { double mean=0, span=0; };
Metrics measure(const std::vector<float>& audio,double rate)
{
    std::vector<double> notes;double previous=0;
    for(int i=int(rate);i<int(audio.size());++i)if(audio[size_t(i-1)]<=0&&audio[size_t(i)]>0)
    {
        double crossing=i-1-audio[size_t(i-1)]/double(audio[size_t(i)]-audio[size_t(i-1)]);
        if(previous>0)notes.push_back(69+12*std::log2(rate/(crossing-previous)/440));previous=crossing;
    }
    check(!notes.empty(),"Missing rendered humanized tone");
    Metrics result;for(double n:notes)result.mean+=n;result.mean/=notes.size();
    std::sort(notes.begin(),notes.end());result.span=notes[notes.size()*95/100]-notes[notes.size()*5/100];return result;
}
void automationTests()
{
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})
    {
        VocalisAudioProcessor p;p.setPlayConfigDetails(channels,channels,rate,64);
        set(p,ParameterIDs::correctionAmount,100);set(p,ParameterIDs::hardTune,100);set(p,ParameterIDs::retuneSpeed,100);p.prepareToPlay(rate,64);
        juce::AudioBuffer<float> buffer(channels,511);juce::MidiBuffer midi;double phase=0,maxStep=0;float previous=0;
        for(int block=0;block<600;++block)
        {
            if(block%13==0)for(int i=0;i<4;++i)set(p,ids[i],float((block*(i+1))%101));
            if(block%71==0)set(p,ParameterIDs::key,float(block%12));
            if(block==400)set(p,ParameterIDs::correctionAmount,0);
            for(int i=0;i<511;++i){float x=float(.04*std::sin(phase));phase+=tau*225/rate;for(int ch=0;ch<channels;++ch)buffer.setSample(ch,i,x);}
            processWithoutAllocations(p,buffer,midi,false);
            for(int i=0;i<511;++i){float x=buffer.getSample(0,i);check(std::isfinite(x)&&std::abs(x)<1,"Humanizer automation instability");maxStep=std::max(maxStep,double(std::abs(x-previous)));previous=x;}
        }
        check(maxStep<.03,"Humanizer automation discontinuity");
        buffer.clear();p.reset();for(int block=0;block<30;++block){buffer.clear();processWithoutAllocations(p,buffer,midi,false);check(buffer.getMagnitude(0,511)==0,"Humanizer silence/reset residue");}
    }
}
}
void runHumanizerTests()
{
    mappingTests();stateTests();automationTests();
    std::ofstream csv("humanizer-measurements.csv");csv<<"rate,channels,mode,case,mean_note,span_semitones\n";
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})
    {
        const auto tight=measure(render(rate,channels,{},true,true),rate);
        const auto human=measure(render(rate,channels,{100,0,0,0},true,true),rate);
        const auto vibrato=measure(render(rate,channels,{0,100,0,0},true,true),rate);
        std::cout<<"Humanizer vibrato "<<rate<<" / "<<channels<<": tight="<<tight.span<<" human="<<human.span<<" vibrato="<<vibrato.span<<" st\n";
        check(human.span>tight.span*1.5 && vibrato.span>tight.span*1.5,"Humanizer did not preserve rendered vibrato");
        const auto centred=measure(render(rate,channels,{},false,true),rate);
        auto softAudio=render(rate,channels,{100,100,100,100},false,true);
        const auto soft=measure(softAudio,rate);
        check(soft.mean>centred.mean+.03 && soft.mean<57.2,"Humanizer note-centre relaxation failed");
        check(softAudio==render(rate,channels,{100,100,100,100},false,false),"Humanizer output depends on host partition");
        for(auto item:{std::pair<const char*,Metrics>{"tight",tight},{"humanize",human},{"vibrato",vibrato},{"centred",centred},{"soft",soft}})
            csv<<rate<<','<<channels<<",LIVE,"<<item.first<<','<<item.second.mean<<','<<item.second.span<<'\n';
        auto quality=measure(render(rate,channels,{100,100,100,100},true,true,PitchShifter::Mode::quality),rate);
        check(std::abs(quality.mean-57)<.25 && quality.span>.2,"QUALITY humanization failed");
        csv<<rate<<','<<channels<<",QUALITY,all,"<<quality.mean<<','<<quality.span<<'\n';
    }
    std::cout<<"PASS: Humanizer mapping, independent refinements, bounds, drift, voiced gating, state, rendered pitch, automation and allocations\n";
}
