#include "../Source/dsp/PitchShifter.h"
#include "../Source/PluginProcessor.h"
#include "../Source/dsp/live/UltraLiveEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <complex>
#include <vector>
#include <iostream>

namespace {
using Clock=std::chrono::steady_clock;
std::vector<float> source(double rate,double hz,bool vowel=false)
{
    std::vector<float> x(size_t(rate),0);
    for(size_t i=0;i<x.size();++i)
    {
        if(!vowel)x[i]=float(.08*std::sin(juce::MathConstants<double>::twoPi*hz*i/rate));
        else for(int h=1;h*hz<8000;++h)
        {
            const double f=h*hz;
            const double gain=.006/h+.02*std::exp(-std::pow((f-700)/180,2))+.012*std::exp(-std::pow((f-2100)/250,2));
            x[i]+=float(gain*std::sin(juce::MathConstants<double>::twoPi*f*i/rate));
        }
    }
    return x;
}
double crossings(const std::vector<float>& x,double rate)
{
    double first=0,last=0;int count=0;
    for(int i=int(rate*.4)+1;i<int(x.size());++i)if(x[size_t(i-1)]<=0&&x[size_t(i)]>0)
    {
        last=i-1-double(x[size_t(i-1)])/(x[size_t(i)]-x[size_t(i-1)]);
        if(count++==0)first=last;
    }
    return count>2?(count-1)*rate/(last-first):0;
}
double bandCentre(const std::vector<float>& x,double rate)
{
    double sum=0,weight=0;
    for(double hz=260;hz<1400;hz+=20)
    {
        std::complex<double> z{},phase{1,0};const auto step=std::polar(1.,-juce::MathConstants<double>::twoPi*hz/rate);
        const int start=int(rate*.5),length=int(rate*.4);
        for(int i=0;i<length;++i){const double w=.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/length);z+=double(x[size_t(start+i)])*w*phase;phase*=step;}
        sum+=hz*std::abs(z);weight+=std::abs(z);
    }
    return sum/(weight+1e-30);
}
struct Engine {
    PitchShifter dsp;
    UltraLiveEngine ultra; int selected=0;
    void prepare(double rate,int channels,float pitch,int mode,float formant=0,float preserve=100)
    {selected=mode;if(mode>=2)ultra.prepare(rate,channels,pitch,formant,preserve,mode==2?30:20);else dsp.prepare(rate,channels,pitch,mode==0?PitchShifter::Mode::live:PitchShifter::Mode::quality,formant,preserve);}
    void process(const float* const* in,float* const* out,int n){if(selected>=2)ultra.process(in,out,n);else dsp.process(in,out,n);}
    int latency()const{return selected>=2?ultra.latencySamples():dsp.latencySamples();}
    void correction(){if(selected>=2){ultra.setCorrectionControls({0,2,85,90,10,80});ultra.setHumanizeControls({20,20,10,20});}else{dsp.setCorrectionControls({0,2,85,90,10,80});dsp.setHumanizeControls({20,20,10,20});}}
};
std::vector<float> render(double rate,float pitch,int mode,const std::vector<float>& input,float formant=0)
{
    Engine engine;engine.prepare(rate,1,pitch,mode,formant);
    std::vector<float> out(input.size());const float* in[]={input.data()};float* output[]={out.data()};
    engine.process(in,output,int(input.size()));return out;
}
}
void runLiveStudy(const juce::String& directory)
{
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile(directory);folder.createDirectory();
    std::ofstream cpu(folder.getChildFile("cpu.csv").getFullPathName().toStdString());
    std::ofstream accuracy(folder.getChildFile("pitch.csv").getFullPathName().toStdString());
    std::ofstream formants(folder.getChildFile("formants.csv").getFullPathName().toStdString());
    std::ofstream timing(folder.getChildFile("timing.csv").getFullPathName().toStdString());
    cpu<<"mode,rate,channels,buffer,latency_samples,latency_ms,mean_us,max_us,mean_deadline_percent,worst_deadline_percent,misses,callbacks,stereo_max_difference,peak,grains,supported_fraction\n";
    accuracy<<"mode,rate,input_hz,shift_st,measured_crossing_hz,crossing_error_cents,detector_hz,detector_error_cents,detector_confidence,dc_mean,rms\n";
    formants<<"mode,rate,formant_st,first_band_centroid_hz\n";
    timing<<"mode,rate,pitch_st,reported_samples,impulse_peak_delay,energy_centroid_delay,first_nonzero_delay,last_nonzero_delay\n";
    for(int mode:{0,1,2,3})for(double rate:{44100.,48000.,88200.,96000.})
    {
        const char* name=mode==0?"LIVE":mode==1?"QUALITY":mode==2?"ULTRA30":"ULTRA20";
        auto vowel=source(rate,123,true);
        for(int channels:{1,2})for(int buffer:{32,64,128,256,512})
        {
            Engine e;e.prepare(rate,channels,-3,mode,1);e.correction();
            std::vector<float> a(size_t(buffer),0),b(a);double sum=0,max=0,peak=0,stereo=0;int misses=0,calls=0;
            for(int warm=0;warm<8;++warm){const float* in[]={vowel.data(),vowel.data()};float* out[]={a.data(),b.data()};e.process(in,out,buffer);}
            for(int offset=0;offset<int(vowel.size());offset+=buffer)
            {
                const int n=std::min(buffer,int(vowel.size())-offset);const float* in[]={vowel.data()+offset,vowel.data()+offset};float* out[]={a.data(),b.data()};
                const auto start=Clock::now();e.process(in,out,n);const double us=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
                sum+=us;max=std::max(max,us);misses+=us>1e6*n/rate;++calls;
                for(int i=0;i<n;++i){peak=std::max(peak,double(std::abs(a[size_t(i)])));if(channels==2)stereo=std::max(stereo,double(std::abs(a[size_t(i)]-b[size_t(i)])));}
            }
            cpu<<name<<','<<rate<<','<<channels<<','<<buffer<<','<<e.latency()<<','<<e.latency()*1000/rate<<','<<sum/calls<<','<<max<<','<<sum/1e4<<','<<max/(1e6*buffer/rate)*100<<','<<misses<<','<<calls<<','<<stereo<<','<<peak<<','<<(mode>=2?e.ultra.grainCount():0)<<','<<(mode>=2?double(e.ultra.supportedSamples())/(vowel.size()+8*buffer):1)<<'\n';
        }
        for(double hz:{80.,110.,220.,440.})for(float pitch:{0.f,1.f,3.f,7.f,12.f,-1.f,-3.f,-7.f,-12.f})
        {
            auto out=render(rate,pitch,mode,source(rate,hz));const double measured=crossings(out,rate);
            PitchDetector detector;detector.prepare(rate);
            double mean=0,energy=0;int count=0;
            for(size_t i=0;i<out.size();++i){detector.processSample(out[i]);if(i>size_t(rate*.4)){mean+=out[i];energy+=out[i]*out[i];++count;}}
            const auto detected=detector.result();
            accuracy<<name<<','<<rate<<','<<hz<<','<<pitch<<','<<measured<<','<<1200*std::log2(measured/(hz*std::pow(2.,pitch/12)))<<','<<detected.hz<<','<<(detected.hz>0?1200*std::log2(detected.hz/(hz*std::pow(2.,pitch/12))):9999)<<','<<detected.confidence<<','<<mean/count<<','<<std::sqrt(energy/count)<<'\n';
        }
        for(float formant:{-3.f,0.f,3.f})formants<<name<<','<<rate<<','<<formant<<','<<bandCentre(render(rate,0,mode,source(rate,120,true),formant),rate)<<'\n';
        for(float pitch:{0.f,3.f})
        {
            std::vector<float> impulse(size_t(rate),0);const int at=int(rate*.25);impulse[size_t(at)]=.5f;
            auto out=render(rate,pitch,mode,impulse);double energy=0,centroid=0,peak=0;int peakAt=0,first=-1,last=-1;
            for(int i=0;i<int(out.size());++i){double x=out[size_t(i)];energy+=x*x;centroid+=i*x*x;if(std::abs(x)>peak){peak=std::abs(x);peakAt=i;}if(std::abs(x)>1e-8){if(first<0)first=i;last=i;}}
            Engine e;e.prepare(rate,1,pitch,mode);timing<<name<<','<<rate<<','<<pitch<<','<<e.latency()<<','<<peakAt-at<<','<<centroid/energy-at<<','<<first-at<<','<<last-at<<'\n';
        }
        std::cout<<"Study completed "<<name<<" "<<rate<<" Hz\n"<<std::flush;
    }
}

// Active processing probe: compare a voiced carrier with/without a tiny marker.
// Unlike an isolated impulse, both ULTRA candidates must actually synthesize
// grains. Nonlinear epoch/detector changes can leave a longer difference tail.
void runActiveLiveProbes()
{
    std::ofstream file("phase11-active-timing.csv");
    file<<"mode,rate,nominal_delay_samples,grains,supported_fraction,peak_delay_samples,energy_centroid_delay_samples,first_difference_delay_samples,last_difference_delay_samples,min_source_age_samples,max_source_age_samples\n";
    for(int mode:{0,1,2,3})for(double rate:{44100.,48000.,88200.,96000.})
    {
        auto carrier=source(rate,220,true),marked=carrier;const int marker=int(rate*.5);marked[size_t(marker)]+=.001f;
        Engine e;e.prepare(rate,1,3,mode);std::vector<float> shifted(marked.size());const float* in[]={marked.data()};float* out[]={shifted.data()};e.process(in,out,int(marked.size()));
        auto reference=render(rate,3,mode,carrier);double energy=0,centroid=0,peak=0;int peakAt=0,first=-1,last=-1;
        for(int i=0;i<int(shifted.size());++i)
        {
            const double x=shifted[size_t(i)]-reference[size_t(i)];energy+=x*x;centroid+=i*x*x;
            if(std::abs(x)>peak){peak=std::abs(x);peakAt=i;}
            if(std::abs(x)>1e-7){if(first<0)first=i;last=i;}
        }
        if(mode>=2&&e.ultra.grainCount()==0)throw std::runtime_error("Active latency probe did not process grains");
        file<<(mode==0?"LIVE":mode==1?"QUALITY":mode==2?"ULTRA30":"ULTRA20")<<','<<rate<<','<<e.latency()<<','<<(mode>=2?e.ultra.grainCount():0)<<','<<(mode>=2?double(e.ultra.supportedSamples())/carrier.size():1)<<','<<peakAt-marker<<','<<centroid/energy-marker<<','<<first-marker<<','<<last-marker<<','<<(mode>=2?e.ultra.minimumSourceAge():0)<<','<<(mode>=2?e.ultra.maximumSourceAge():0)<<'\n';
    }
}
void writeStudyWav(const juce::File& file,const std::vector<float>& x)
{
    auto stream=file.createOutputStream();if(!stream)throw std::runtime_error("Cannot create audition WAV");stream->setPosition(0);stream->truncate();
    juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> destination=std::move(stream);
    auto writer=format.createWriterFor(destination,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(1).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("Cannot create WAV writer");const float* samples[]={x.data()};
    if(!writer->writeFromFloatArrays(samples,1,int(x.size())))throw std::runtime_error("Cannot write audition WAV");
}
void runLiveAuditions()
{
    auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("phase11-audition");folder.createDirectory();
    constexpr double rate=48000;std::vector<float> x(48000*8,0);juce::Random noise(1109);double phase=0;
    for(size_t i=0;i<x.size();++i)
    {
        const double t=i/rate,local=t-std::floor(t);const int section=int(t);
        double hz=120;
        if(section==1)hz=220*std::pow(2.,.25*std::sin(juce::MathConstants<double>::twoPi*5*t)/12);
        if(section==2)hz=110+220*local;
        if(section==3)hz=int(local*8)%2?277:220;
        if(section==4)hz=80;
        if(section==5)hz=440;
        if(section>=6)hz=220;
        phase+=juce::MathConstants<double>::twoPi*hz/rate;double value=0;
        for(int h=1;h*hz<8000;++h){double f=h*hz;double gain=.008/h+.018*std::exp(-std::pow((f-700)/180,2))+.01*std::exp(-std::pow((f-2100)/250,2));value+=gain*std::sin(h*phase);}
        if(section==6)value+=(noise.nextFloat()-.5)*.06;
        if(section==7)value=local<.15?.1*std::exp(-local*35)*(noise.nextFloat()-.5):(local>.45?(noise.nextFloat()-.5)*.06:value);
        const double envelope=std::min(1.,std::min(local/.02,(1-local)/.02));x[i]=float(value*envelope);
    }
    writeStudyWav(folder.getChildFile("00-source.wav"),x);
    for(int mode:{0,1,2,3})for(float pitch:{3.f,-3.f})
    {
        const juce::String name=mode==0?"LIVE":mode==1?"QUALITY":mode==2?"ULTRA30":"ULTRA20";
        writeStudyWav(folder.getChildFile(name+(pitch>0?"-plus3":"-minus3")+".wav"),render(rate,pitch,mode,x,pitch<0?2.f:0.f));
    }
    std::cout<<"Active timing probes and deterministic audition files generated\n";
}

void runLiveActiveCpu()
{
    std::ofstream file("phase11-active-cpu.csv");
    file<<"mode,rate,channels,buffer,mean_us,max_us,mean_deadline_percent,misses,callbacks,grains,supported_fraction\n";
    // Separate matched-input study: 220 Hz lies within BOTH experimental ranges.
    // The main 123 Hz study intentionally exposes ULTRA20's lack of coverage.
    for(int mode:{0,1,2,3})for(double rate:{44100.,48000.,88200.,96000.})for(int buffer:{32,64,128,256,512})
    {
        auto input=source(rate,220,true);Engine e;e.prepare(rate,2,-3,mode,1);e.correction();
        std::vector<float> a(size_t(buffer),0),b(a);double sum=0,worst=0;int calls=0,misses=0;
        for(int offset=0;offset<int(input.size());offset+=buffer)
        {
            const int count=std::min(buffer,int(input.size())-offset);const float* in[]={input.data()+offset,input.data()+offset};float* out[]={a.data(),b.data()};
            const auto start=Clock::now();e.process(in,out,count);const double us=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
            sum+=us;worst=std::max(worst,us);misses+=us>count*1e6/rate;++calls;
        }
        file<<(mode==0?"LIVE":mode==1?"QUALITY":mode==2?"ULTRA30":"ULTRA20")<<','<<rate<<",2,"<<buffer<<','<<sum/calls<<','<<worst<<','<<sum/10000<<','<<misses<<','<<calls<<','<<(mode>=2?e.ultra.grainCount():0)<<','<<(mode>=2?double(e.ultra.supportedSamples())/input.size():1)<<'\n';
    }
    std::cout<<"Matched 220 Hz active CPU comparison complete\n";
}
