#include "../Source/dsp/live/UltraLiveEngine.h"
#include "reference/Phase11UltraLiveEngine.h"
#include "../Source/dsp/PitchShifter.h"
#include "../Source/dsp/BloomMapping.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
#include <iostream>
#include <complex>
#include <numeric>
#include <iomanip>

namespace {
constexpr double qaRate=48000;
struct Clip { juce::String name,category; std::vector<float> samples; double steadyHz=0; };
struct Case { juce::String name; float pitch=0,formant=0,amount=0,speed=50,transition=50,hard=0,human=0,bloom=0; };
struct Render { std::vector<float> samples; int delay=0; double supported=1; uint64_t grains=0; unsigned octaveHolds=0; };
struct Stats { double rms=0,peak=0,dc=0,step=0,medianHz=0,cents=0; int voiced=0,frames=0,octaves=0,clipped=0; };
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void wav(const juce::File& file,const std::vector<float>& data)
{
    file.getParentDirectory().createDirectory();auto stream=file.createOutputStream();require(bool(stream),"Cannot create QA WAV");stream->setPosition(0);stream->truncate();
    std::unique_ptr<juce::OutputStream> base=std::move(stream);juce::WavAudioFormat format;
    auto writer=format.createWriterFor(base,juce::AudioFormatWriterOptions{}.withSampleRate(qaRate).withNumChannels(1).withBitsPerSample(24));
    require(bool(writer),"Cannot create QA writer");const float* p[]={data.data()};require(writer->writeFromFloatArrays(p,1,int(data.size())),"QA WAV write failure");
}
Clip synth(double f0,const juce::String& reg,int scenario)
{
    const char* names[]={"vowel","vibrato","legato-slide","fast-notes","quiet-breathy","loud","consonants"};
    Clip clip;clip.name=reg+"-"+names[scenario];clip.category=reg;clip.samples.resize(size_t(qaRate*2));
    if(scenario==0||scenario==5)clip.steadyHz=f0;
    juce::Random noise(12100+scenario);double phase=0,previousNoise=0;
    for(size_t i=0;i<clip.samples.size();++i)
    {
        const double t=i/qaRate;double hz=f0;
        if(scenario==1)hz*=std::pow(2.,.3*std::sin(juce::MathConstants<double>::twoPi*5*t)/12);
        if(scenario==2)hz*=std::pow(2.,(-4+8*t/2)/12);
        if(scenario==3){constexpr double notes[]={0,3,-2,7,0,-5,2,0};hz*=std::pow(2.,notes[int(t*4)%8]/12);}
        phase+=juce::MathConstants<double>::twoPi*hz/qaRate;double x=0;
        for(int h=1;h*hz<8000;++h)
        {
            const double f=h*hz;
            const double amplitude=.005/h+.019*std::exp(-std::pow((f-700)/160,2))+.011*std::exp(-std::pow((f-2100)/240,2));
            x+=amplitude*std::sin(h*phase+.11*h*h);
        }
        const double random=noise.nextFloat()-.5,high=random-previousNoise;previousNoise=random;
        if(scenario==4)x=.15*x+.003*random;
        if(scenario==5)x*=4;
        if(scenario==6)
        {
            // Synthetic S/F/SH-like noise and T/K/P-like bursts, not claimed phonemes.
            if(t>.35&&t<.8)x=.025*high;
            if(t>=.8&&t<1.05)x=.025*random;
            if(t>=1.4&&t<1.65)x=.10*random*std::exp(-(t-1.4)*60);
        }
        const double envelope=std::min(1.,std::min(t/.02,(2-t)/.02));clip.samples[i]=float(x*envelope);
    }
    return clip;
}
std::vector<Clip> corpus(const juce::File& folder)
{
    std::vector<Clip> clips;const char* registers[]={"low","mid","high"};const double f0[]={120,200,330};
    for(int reg=0;reg<3;++reg)for(int scenario=0;scenario<7;++scenario)clips.push_back(synth(f0[reg],registers[reg],scenario));
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    for(auto file:folder.getChildFile("spoken").findChildFiles(juce::File::findFiles,false,"*.wav"))
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        require(reader&&reader->lengthInSamples>1000,"Spoken corpus file is empty or unreadable");
        juce::AudioBuffer<float> data(1,int(reader->lengthInSamples));reader->read(&data,0,data.getNumSamples(),0,true,false);
        Clip clip;clip.name=file.getFileNameWithoutExtension();clip.category="generated-spoken";
        const double step=reader->sampleRate/qaRate;clip.samples.resize(size_t(data.getNumSamples()/step));
        for(size_t i=0;i<clip.samples.size();++i)
        {
            const double p=i*step;const int n=std::min(int(p),data.getNumSamples()-1),next=std::min(n+1,data.getNumSamples()-1);
            clip.samples[i]=data.getSample(0,n)+float(p-n)*(data.getSample(0,next)-data.getSample(0,n));
        }
        clips.push_back(std::move(clip));
    }
    folder.createDirectory();std::ofstream manifest(folder.getChildFile("corpus.csv").getFullPathName().toStdString());
    manifest<<"name,category,seconds,sample_rate,steady_ground_truth_hz,provenance\n";
    for(const auto& clip:clips){wav(folder.getChildFile("sources/"+clip.name+".wav"),clip.samples);manifest<<clip.name<<','<<clip.category<<','<<clip.samples.size()/qaRate<<",48000,"<<clip.steadyHz<<','<<(clip.category=="generated-spoken"?"local Windows SAPI; see spoken/provenance.csv":"deterministic additive source; VocalValidation.cpp")<<'\n';}
    return clips;
}
Render render(const Clip& clip,const Case& setting,int mode)
{
    Render result;
    if(mode==0){result.samples=clip.samples;return result;}
    UltraLiveEngine ultra;Phase11UltraLiveEngine old;PitchShifter spectral;
    const auto character=BloomMapping::apply(setting.bloom,{setting.human,setting.human,setting.human*.2f,setting.human},setting.formant);
    const PitchCorrector::Controls correction{0,2,setting.amount,setting.speed,setting.transition,setting.hard};
    const auto prepare=[&](auto& engine){engine.prepare(qaRate,1,setting.pitch,character.formant,100);engine.setCorrectionControls(correction);engine.setHumanizeControls(character.humanizer);};
    if(mode==1)prepare(old);else if(mode==2)prepare(ultra);else
    {spectral.prepare(qaRate,1,setting.pitch,mode==3?PitchShifter::Mode::live:PitchShifter::Mode::quality,character.formant,100);spectral.setCorrectionControls(correction);spectral.setHumanizeControls(character.humanizer);}
    result.delay=mode==1?old.latencySamples():mode==2?ultra.latencySamples():spectral.latencySamples();
    std::vector<float> input=clip.samples;input.resize(input.size()+size_t(qaRate*.3),0);std::vector<float> output(input.size());
    for(int offset=0;offset<int(input.size());offset+=128)
    {
        const int n=std::min(128,int(input.size())-offset);const float* in[]={input.data()+offset};float* out[]={output.data()+offset};
        if(mode==1)old.process(in,out,n);else if(mode==2)ultra.process(in,out,n);else spectral.process(in,out,n);
    }
    for(float x:output)require(std::isfinite(x)&&std::abs(x)<4,"Unsafe QA render: nonfinite or runaway gain");
    result.samples.assign(output.begin()+result.delay,output.begin()+result.delay+clip.samples.size());
    if(mode==1){result.grains=old.grainCount();result.supported=double(old.supportedSamples())/input.size();}
    if(mode==2){result.grains=ultra.grainCount();result.supported=double(ultra.supportedSamples())/input.size();result.octaveHolds=ultra.heldOctaveFrames();}
    return result;
}
Stats stats(const std::vector<float>& x,double expected=0)
{
    Stats s;double energy=0;float previous=0;std::vector<float> frequencies;PitchDetector detector;detector.prepare(qaRate);
    for(size_t i=0;i<x.size();++i)
    {
        require(std::isfinite(x[i]),"Nonfinite QA statistic input");energy+=x[i]*x[i];s.dc+=x[i];s.peak=std::max(s.peak,double(std::abs(x[i])));
        s.clipped+=std::abs(x[i])>=1;s.step=std::max(s.step,double(std::abs(x[i]-previous)));previous=x[i];
        if(detector.processSample(x[i])&&i>size_t(qaRate*.2)&&i+size_t(qaRate*.1)<x.size())
        {++s.frames;const auto r=detector.result();if(r.hz>0){++s.voiced;frequencies.push_back(r.hz);if(expected>0&&std::abs(std::abs(1200*std::log2(r.hz/expected))-1200)<100)++s.octaves;}}
    }
    s.rms=std::sqrt(energy/x.size());s.dc/=x.size();
    if(!frequencies.empty()){std::sort(frequencies.begin(),frequencies.end());s.medianHz=frequencies[frequencies.size()/2];}
    s.cents=expected>0&&s.medianHz>0?1200*std::log2(s.medianHz/expected):0;return s;
}
double centroid(const std::vector<float>& x)
{
    double sum=0,weight=0;const int start=24000,count=24000;
    for(double hz=250;hz<1400;hz+=20)
    {
        std::complex<double> value{},phase{1,0};const auto step=std::polar(1.,-juce::MathConstants<double>::twoPi*hz/qaRate);
        for(int i=0;i<count;++i){value+=double(x[size_t(start+i)])*(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/count))*phase;phase*=step;}
        const double power=std::norm(value);sum+=power*hz;weight+=power;
    }
    return sum/(weight+1e-30);
}
}
void runVocalValidation()
{
    auto root=juce::File::getCurrentWorkingDirectory();auto out=root.getChildFile("phase12-qa");out.createDirectory();
    auto clips=corpus(root.getChildFile("phase12-corpus"));
    std::vector<Case> cases{{"Natural",0,0,70,40,50,0,50},{"Pop",0,0,95,75,25,55,25},{"HardTune",0,0,100,100,0,100,0}};
    for(float shift:{1.f,2.f,3.f,4.f,-1.f,-2.f,-3.f,-4.f,6.f,-6.f}){Case c;c.name="Pitch"+juce::String(shift);c.pitch=shift;cases.push_back(c);}
    for(float formant:{1.f,2.f,3.f,-1.f,-2.f,-3.f}){Case c;c.name="Formant"+juce::String(formant);c.formant=formant;cases.push_back(c);}
    for(float human:{0.f,25.f,50.f,75.f,100.f}){Case c;c.name="Humanize"+juce::String(human);c.human=human;c.amount=95;c.speed=85;c.hard=80;cases.push_back(c);}
    for(float bloom:{0.f,50.f,100.f}){Case c;c.name="Bloom"+juce::String(bloom);c.bloom=bloom;c.amount=90;c.speed=70;c.human=30;cases.push_back(c);}
    std::ofstream metrics(out.getChildFile("metrics.csv").getFullPathName().toStdString()),key(out.getChildFile("ANSWER-KEY.csv").getFullPathName().toStdString()),worksheet(out.getChildFile("ListeningWorksheet.csv").getFullPathName().toStdString()),matching(out.getChildFile("matching.csv").getFullPathName().toStdString());
    metrics<<"test,clip,case,engine,nominal_delay_samples,supported_fraction,grains,octave_holds,rms,peak,dc,max_adjacent_step,clipped_samples,observed_hz,expected_hz,cents,voiced_frames,frames,octave_error_frames,formant_band_centroid\n";
    key<<"test,clip,case,A,B,C\n";worksheet<<"test,clip,case,file,pitch_stability,naturalness,consonants,vibrato,transients,graininess,metallic_character,timbre,formants,perceived_delay,notes\n";
    matching<<"test,file,raw_rms,raw_peak,linear_gain,matched_rms,matched_peak\n";
    const char* names[]={"DRY","PHASE11_ULTRA30","ULTRA30","LIVE","QUALITY"};int test=0;
    juce::Random assignment(12012026);
    for(const auto& clip:clips)for(const auto& setting:cases)
    {
        const bool isFormant=setting.name.startsWith("Formant"),isHuman=setting.name.startsWith("Humanize")||setting.name.startsWith("Bloom");
        if(isFormant&&!clip.name.endsWith("-vowel"))continue;
        if(isHuman&&!clip.name.endsWith("-vibrato"))continue;
        const auto id="Test"+juce::String(++test).paddedLeft('0',4);
        std::array<Render,5> audio;std::array<Stats,5> observations;
        for(int mode=0;mode<5;++mode)
        {
            audio[size_t(mode)]=render(clip,setting,mode);
            const double expected=clip.steadyHz>0&&setting.amount==0?clip.steadyHz*std::pow(2.,mode==0?0:setting.pitch/12):0;
            auto s=stats(audio[size_t(mode)].samples,expected);observations[size_t(mode)]=s;
            const auto& a=audio[size_t(mode)];
            metrics<<id<<','<<clip.name<<','<<setting.name<<','<<names[mode]<<','<<a.delay<<','<<a.supported<<','<<a.grains<<','<<a.octaveHolds<<','<<s.rms<<','<<s.peak<<','<<s.dc<<','<<s.step<<','<<s.clipped<<','<<s.medianHz<<','<<expected<<','<<s.cents<<','<<s.voiced<<','<<s.frames<<','<<s.octaves<<','<<(isFormant?centroid(a.samples):0)<<'\n';
        }
        std::array<int,3> order{{2,3,4}};for(int i=2;i>0;--i)std::swap(order[size_t(i)],order[size_t(assignment.nextInt(i+1))]);
        key<<id<<','<<clip.name<<','<<setting.name<<','<<names[order[0]]<<','<<names[order[1]]<<','<<names[order[2]]<<'\n';
        // RMS match with one common target, constrained by every candidate's
        // peak and a +6 dB maximum boost. Never compress/limit or hide a raw dropout.
        double target=.05;for(int mode:{0,2,3,4}){const auto& s=observations[size_t(mode)];require(s.rms>1e-9,"Silent comparison candidate");target=std::min(target,s.rms*std::min(2.,.89/std::max(s.peak,1e-9)));}
        for(int slot=0;slot<4;++slot)
        {
            const int mode=slot==3?0:order[size_t(slot)];const auto& raw=observations[size_t(mode)];const double gain=target/raw.rms;
            auto samples=audio[size_t(mode)].samples;for(auto& x:samples)x*=float(gain);
            const auto filename=id+"-"+(slot==3?juce::String("Reference"):juce::String::charToString(char('A'+slot)))+".wav";
            wav(out.getChildFile("blind/"+filename),samples);
            matching<<id<<','<<filename<<','<<raw.rms<<','<<raw.peak<<','<<gain<<','<<raw.rms*gain<<','<<raw.peak*gain<<'\n';
            worksheet<<id<<','<<clip.name<<','<<setting.name<<','<<filename<<",,,,,,,,,,,\n";
        }
        if(test%20==0)std::cout<<"QA comparisons rendered: "<<test<<"\n"<<std::flush;
    }
    std::cout<<"Vocal QA complete: "<<clips.size()<<" source clips, "<<test<<" matched comparisons, "<<test*4<<" blind/reference WAVs\n";
}

void runVocalFocusedTests()
{
    auto out=juce::File::getCurrentWorkingDirectory().getChildFile("phase12-qa");out.createDirectory();
    std::ofstream file(out.getChildFile("focused.csv").getFullPathName().toStdString());
    file<<"input_hz,case,engine,expected_hz,observer_in_range,observed_hz,cents,rms,peak,octave_frames,supported_fraction,grains,formant_centroid\n";
    for(double hz:{80.,90.,100.,110.,120.,150.,200.,330.,440.})
    {
        auto clip=synth(hz,"focused",0);
        std::vector<Case> cases;
        for(float shift:{1.f,2.f,3.f,4.f,-1.f,-2.f,-3.f,-4.f,6.f,-6.f}){Case c;c.name="Pitch"+juce::String(shift);c.pitch=shift;cases.push_back(c);}
        for(float f:{0.f,1.f,2.f,3.f,-1.f,-2.f,-3.f}){Case c;c.name="Formant"+juce::String(f);c.formant=f;cases.push_back(c);}
        for(const auto& c:cases)for(int mode:{1,2})
        {
            const auto a=render(clip,c,mode);const double expected=hz*std::pow(2.,c.pitch/12);const auto s=stats(a.samples,expected);
            file<<hz<<','<<c.name<<','<<(mode==1?"PHASE11_ULTRA30":"ULTRA30")<<','<<expected<<','<<(expected>=65&&expected<=1100)<<','<<s.medianHz<<','<<s.cents<<','<<s.rms<<','<<s.peak<<','<<s.octaves<<','<<a.supported<<','<<a.grains<<','<<(c.name.startsWith("Formant")?centroid(a.samples):0)<<'\n';
            require(a.delay==1440,"ULTRA30 fixed delay changed");require(s.peak<1&&s.clipped==0,"Focused render clipped");
        }
    }
    std::ofstream transitions(out.getChildFile("transitions.csv").getFullPathName().toStdString());
    transitions<<"f0,pitch,engine,unvoiced_relative_error,max_adjacent_step,peak\n";
    for(double hz:{120.,200.,330.})for(float shift:{-3.f,3.f})for(int mode:{1,2,3,4})
    {
        auto clip=synth(hz,"transition",6);Case c;c.pitch=shift;auto a=render(clip,c,mode);double error=0,power=0;
        // Fixed unvoiced regions after each onset; raw input and output share the
        // nominal delay compensation. This measures preservation, not naturalness.
        for(int i=int(qaRate*.40);i<int(qaRate*1.02);++i){double d=a.samples[size_t(i)]-clip.samples[size_t(i)];error+=d*d;power+=clip.samples[size_t(i)]*clip.samples[size_t(i)];}
        const auto s=stats(a.samples);const char* names[]={"","PHASE11_ULTRA30","ULTRA30","LIVE","QUALITY"};
        transitions<<hz<<','<<shift<<','<<names[mode]<<','<<std::sqrt(error/(power+1e-30))<<','<<s.step<<','<<s.peak<<'\n';
    }
    VocalTrackingGuard guard;guard.reset();guard.update({100,.99f});guard.update({100,.99f});
    require(guard.frequency()==100,"Guard failed acquisition");guard.update({200,.99f});require(guard.frequency()==100,"Isolated octave-up escaped guard");
    guard.update({100,.99f});guard.update({50,.99f});require(guard.frequency()==100,"Isolated octave-down escaped guard");
    guard.update({100,.99f});for(int i=0;i<3;++i)guard.update({200,.99f});require(guard.frequency()==200,"Guard blocked genuine sustained octave leap");
    guard.update({});guard.update({});require(guard.frequency()==0,"Guard froze old pitch indefinitely");
    std::ofstream ambiguity(out.getChildFile("octave-ambiguity.csv").getFullPathName().toStdString());
    ambiguity<<"fundamental_hz,second_harmonic_gain,raw_octave_frames,guard_octave_frames,frames,held_frames\n";
    for(double hz:{80.,100.,150.,220.})for(double second:{.5,2.,8.})
    {
        PitchDetector detector;detector.prepare(qaRate,PitchDetector::Profile::fastExperimental);guard.reset();int frames=0,rawWrong=0,guardWrong=0;
        for(int i=0;i<48000;++i)
        {
            const double phase=juce::MathConstants<double>::twoPi*hz*i/qaRate;
            // Established fundamental followed by a weak-fundamental segment.
            const float x=float(.02*std::sin(phase)+.02*(i<12000?.5:second)*std::sin(2*phase));
            if(detector.processSample(x)){auto r=detector.result();guard.update(r);if(i>14400){++frames;rawWrong+=std::abs(r.hz-2*hz)<hz*.05;guardWrong+=std::abs(guard.frequency()-2*hz)<hz*.05;}}
        }
        ambiguity<<hz<<','<<second<<','<<rawWrong<<','<<guardWrong<<','<<frames<<','<<guard.heldOctaveFrames()<<'\n';
    }
    std::cout<<"PASS: focused output safety/fixed delay and octave-hold/release behavior; low-F0, formant and ambiguity metrics recorded separately\n";
}
