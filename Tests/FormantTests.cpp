#include "../Source/PluginProcessor.h"
#include "../Source/dsp/FormantProcessor.h"
#include <chrono>
#include <complex>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
void processWithoutAllocations(juce::AudioProcessor&, juce::AudioBuffer<float>&, juce::MidiBuffer&, bool);
void processDspWithoutAllocations(PitchShifter&, const float* const*, float* const*, int);
namespace {
void check(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
void set(VocalisAudioProcessor& p, const char* id, float v) { auto* a=p.parameters.getParameter(id); a->setValueNotifyingHost(a->convertTo0to1(v)); }
std::vector<float> vowel(double rate) {
    std::vector<float> a(static_cast<size_t>(rate));
    for(int h=1; h<=60; ++h) {
        double f=h*120.0;
        double gain=0.025/h + 0.025*std::exp(-std::pow((f-700)/180,2)) + 0.014*std::exp(-std::pow((f-2100)/250,2));
        for(size_t i=0;i<a.size();++i) a[i]+=float(gain*std::sin(juce::MathConstants<double>::twoPi*f*i/rate));
    } return a;
}
double magnitude(const std::vector<float>& a,double rate,double f) {
    std::complex<double> z{}, phase{1,0}, window{1,0};
    int n=int(rate*0.4), start=int(rate*0.5);
    auto rotation=std::polar(1.0,-juce::MathConstants<double>::twoPi*f/rate);
    auto windowRotation=std::polar(1.0,juce::MathConstants<double>::twoPi/n);
    for(int i=0;i<n;++i) {
        double w=0.5-0.5*window.real();
        z += double(a[size_t(start+i)])*w*phase;
        phase*=rotation; window*=windowRotation;
    } return std::abs(z);
}
double centroid(const std::vector<float>& a,double rate,double fundamental) {
    double sum=0, weight=0;
    juce::ignoreUnused(fundamental);
    for(double f=260;f<1400;f+=10) {
        double m=magnitude(a,rate,f); sum+=m*f; weight+=m;
    } return sum/weight;
}
double pitchError(const std::vector<float>& a,double rate,double expected) {
    double best=0, frequency=expected;
    for(int step=-80;step<=80;++step) {
        double f=expected*std::pow(2.0,step/1200.0), score=0;
        for(int h=1;h<=12;++h) score+=magnitude(a,rate,f*h);
        if(score>best) {best=score;frequency=f;}
    } return 1200*std::log2(frequency/expected);
}
std::vector<float> render(double rate,int channels,float pitch,float formant,float preserve,PitchShifter::Mode mode) {
    auto source=vowel(rate); std::vector<float> left(source.size()),right(source.size());
    PitchShifter dsp; dsp.prepare(rate,channels,pitch,mode,formant,preserve);
    check(dsp.latencySamples()==int(rate*(mode==PitchShifter::Mode::live?0.048:0.12))
        +(mode==PitchShifter::Mode::live&&rate>=88200?int(rate*.003):0)+64,"Formant latency changed");
    const int sizes[]={1,17,64,127,256,511,1024}; int index=0;
    for(int pos=0;pos<int(source.size());) {
        int n=std::min(sizes[index++%7],int(source.size())-pos);
        const float* in[]={source.data()+pos,source.data()+pos}; float* out[]={left.data()+pos,right.data()+pos};
        processDspWithoutAllocations(dsp,in,out,n); pos+=n;
    }
    double errorEnergy=0, signalEnergy=0;
    for(size_t i=0;i<left.size();++i) {
        check(std::isfinite(left[i])&&std::abs(left[i])<2,"Formant nonfinite/runaway output");
        if(channels==2) {errorEnergy+=std::pow(left[i]-right[i],2);signalEnergy+=left[i]*left[i];}
    }
    if(channels==2) { double mismatch=std::sqrt(errorEnergy/(signalEnergy+1e-30)); if(mismatch>1e-4) std::cout<<"Stereo mismatch "<<int(mode)<<" rate="<<rate<<" pitch="<<pitch<<" formant="<<formant<<" relative="<<mismatch<<std::endl; check(mismatch<0.005,"Formant stereo mismatch >0.5% RMS"); }
    // Output must be invariant to host partitioning, including the final short block.
    PitchShifter fixed; fixed.prepare(rate,channels,pitch,mode,formant,preserve);
    std::vector<float> fixedLeft(source.size()), fixedRight(source.size());
    for(int pos=0;pos<int(source.size());pos+=64) {
        int n=std::min(64,int(source.size())-pos);
        const float* in[]={source.data()+pos,source.data()+pos};
        float* out[]={fixedLeft.data()+pos,fixedRight.data()+pos};
        processDspWithoutAllocations(fixed,in,out,n);
    }
    check(left==fixedLeft && (channels==1 || right==fixedRight), "Host buffer partition changed formant output");
    return left;
}
}
void runFormantTests() {
    VocalisAudioProcessor fresh;
    check(fresh.parameters.getRawParameterValue("formantPreserve")->load()==100,"Preserve default");
    for(float preserve:{0.f,0.5f,1.f}) {
        auto s=FormantProcessor::settings(4,{1,preserve});
        check(s.compensatePitch&&s.semitones==1+(1-preserve)*4,"Preserve mapping");
    }
    auto legacy=fresh.parameters.copyState();
    for(int i=legacy.getNumChildren()-1;i>=0;--i) if(legacy.getChild(i).getProperty("id").toString().startsWith("formant")) legacy.removeChild(i,nullptr);
    juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),data);
    fresh.setStateInformation(data.getData(),int(data.getSize()));
    check(fresh.parameters.getRawParameterValue("formantPreserve")->load()==0,"Legacy timbre migration");
    std::ofstream csv("formant-quality.csv"); csv<<"mode,rate,channels,pitch,formant,preserve,pitch_error_cents,envelope_centroid_hz\n";
    double worst=0;
    for(auto mode:{PitchShifter::Mode::live,PitchShifter::Mode::quality})
    for(double rate:{44100.,48000.,96000.}) for(int channels:{1,2}) {
        double minus=0,zero=0,plus=0;
        for(float f:{0.f,1.f,3.f,6.f,-1.f,-3.f,-6.f}) {
            auto a=render(rate,channels,0,f,100,mode); double cents=pitchError(a,rate,120), c=centroid(a,rate,120);
            worst=std::max(worst,std::abs(cents)); check(std::abs(cents)<=3,"Formant-only pitch deviation");
            if(f==-3) minus=c; if(f==0) zero=c; if(f==3) plus=c;
            csv<<(mode==PitchShifter::Mode::live?"LIVE":"QUALITY")<<','<<rate<<','<<channels<<",0,"<<f<<",100,"<<cents<<','<<c<<'\n';
        }
        check(minus<zero-40&&plus>zero+40,"Formant envelope did not move");
        for(auto pair:{std::pair<float,float>{3.f,2.f},{-4.f,-2.f},{12.f,6.f},{-12.f,-6.f}}) {
            auto a=render(rate,channels,pair.first,pair.second,100,mode);
            double cents=pitchError(a,rate,120*std::pow(2.0,pair.first/12.0));
            std::cout << "Combined " << rate << " / " << pair.first << " / " << pair.second << ": " << cents << " cents\n";
            check(std::abs(cents)<=25,"Combined pitch deviation exceeds 25 cents");
            csv<<(mode==PitchShifter::Mode::live?"LIVE":"QUALITY")<<','<<rate<<','<<channels<<','<<pair.first<<','<<pair.second<<",100,"<<cents<<','<<centroid(a,rate,120*std::pow(2.0,pair.first/12.0))<<'\n';
        }
        double c[3]; int j=0;
        for(float preserve:{0.f,50.f,100.f}) {
            auto a=render(rate,channels,3,0,preserve,mode);
            c[j++]=centroid(a,rate,120*std::pow(2.,0.25));
            csv<<(mode==PitchShifter::Mode::live?"LIVE":"QUALITY")<<','<<rate<<','<<channels<<",3,0,"<<preserve<<','<<pitchError(a,rate,120*std::pow(2.,0.25))<<','<<c[j-1]<<'\n';
        }
        std::cout<<"Preserve centroids "<<zero<<" -> "<<c[0]<<", "<<c[1]<<", "<<c[2]<<std::endl;
        check(c[0]>c[1]&&c[1]>c[2]&&std::abs(c[2]-zero)<std::abs(c[0]-zero),"Preserve envelope behavior");
    }
    for(double rate:{44100.,48000.,96000.}) for(int channels:{1,2}) {
        VocalisAudioProcessor p; p.setPlayConfigDetails(channels,channels,rate,64); p.prepareToPlay(rate,64);
        juce::AudioBuffer<float> b(channels,1024); juce::MidiBuffer midi; juce::Random random(77);
        for(int block=0;block<240;++block) {
            if(block%7==0) {set(p,"pitch",float(block%25-12));set(p,"formantShift",float(block%13-6));set(p,"formantPreserve",float(block%101));}
            int n=1+block*73%1024; b.setSize(channels,n,false,false,true);
            for(int i=0;i<n;++i) {float x=(random.nextFloat()-.5f)*.1f;for(int ch=0;ch<channels;++ch)b.setSample(ch,i,x);}
            processWithoutAllocations(p,b,midi,false);
            for(int i=0;i<n;++i) for(int ch=0;ch<channels;++ch) check(std::isfinite(b.getSample(ch,i))&&std::abs(b.getSample(ch,i))<2,"Automation unstable");
        }
    }
    // Rapid control movement, recovery to exact unity, and silence in both engines.
    double largestStep=0;
    for(auto mode:{PitchShifter::Mode::live,PitchShifter::Mode::quality})
    for(double rate:{44100.,48000.,96000.}) {
        PitchShifter dsp; dsp.prepare(rate,2,0,mode,0,100);
        std::vector<float> input(256),left(256),right(256); float previous=0;
        for(int block=0;block<600;++block) {
            if(block<240 && block%13==0) {dsp.setFormants(block%26==0?6.f:-6.f,float(block%101));dsp.setSemitones(block%26==0?3.f:-4.f);}
            if(block==240) {dsp.setFormants(0,100);dsp.setSemitones(0);}
            for(int i=0;i<256;++i) input[size_t(i)]=float(.05*std::sin(juce::MathConstants<double>::twoPi*220*(block*256+i)/rate));
            const float* in[]={input.data(),input.data()};float* out[]={left.data(),right.data()};
            processDspWithoutAllocations(dsp,in,out,256);
            for(int i=0;i<256;++i) {
                largestStep=std::max(largestStep,double(std::abs(left[size_t(i)]-previous)));previous=left[size_t(i)];
                if(block>400) {float clean=float(.05*std::sin(juce::MathConstants<double>::twoPi*220*(block*256+i-dsp.latencySamples())/rate));check(left[size_t(i)]==clean,"Formant return to neutral timing");}
            }
        }
        dsp.reset(0,6,100);std::fill(input.begin(),input.end(),0.0f);
        for(int i=0;i<100;++i) {const float* in[]={input.data(),input.data()};float* out[]={left.data(),right.data()};processDspWithoutAllocations(dsp,in,out,256);for(float x:left)check(x==0,"Formant silence/reset residue");}
    }
    check(largestStep<0.02,"Rapid transform automation discontinuity");
    std::cout<<"Maximum sine step during rapid transform automation: "<<largestStep<<"\n";
    std::cout<<"Formant-only worst pitch deviation: "<<worst<<" cents (1-cent grid); envelope direction and preservation passed\n";
}
void runFormantBenchmark() {
    std::ofstream csv("formant-cpu.csv"); csv<<"mode,rate,buffer,case,latency,cpu_percent,p99_us,max_us,deadline_misses\n";
    struct Case {const char* name; float pitch,formant,preserve;};
    for(auto mode:{PitchShifter::Mode::live,PitchShifter::Mode::quality}) for(double rate:{44100.,48000.,96000.}) {
        int duration=int(rate*3);std::vector<float> source(size_t(duration),0);juce::Random random(42);
        for(int i=0;i<duration;++i) {double t=i/rate,v=0;for(int h=1;h<=16;++h)v+=std::sin(juce::MathConstants<double>::twoPi*h*(130*t+.08*std::sin(31.4*t)))/h;source[size_t(i)]=float(v*.08)+(random.nextFloat()-.5f)*.002f;}
        for(int buffer:{64,256}) for(Case c: {Case{"neutral",0,0,100},{"legacyPitch",-5.5f,0,0},{"pitch",-5.5f,0,100},{"formant",0,3,100},{"combined",-5.5f,-2,100}}) {
            PitchShifter dsp;dsp.prepare(rate,2,c.pitch,mode,c.formant,c.preserve);
            std::vector<float> left(size_t(buffer),0),right(left);std::vector<double> times;times.reserve(size_t(duration/buffer+1));double sum=0;int misses=0;
            for(int pos=0;pos<duration;pos+=buffer){int n=std::min(buffer,duration-pos);const float* in[]={source.data()+pos,source.data()+pos};float* out[]={left.data(),right.data()};auto start=std::chrono::steady_clock::now();dsp.process(in,out,n);double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();times.push_back(us);sum+=us;if(us>n*1e6/rate)++misses;}
            std::sort(times.begin(),times.end());csv<<(mode==PitchShifter::Mode::live?"LIVE":"QUALITY")<<','<<rate<<','<<buffer<<','<<c.name<<','<<dsp.latencySamples()<<','<<sum/30000<<','<<times[size_t(times.size()*.99)]<<','<<times.back()<<','<<misses<<'\n';csv.flush();
        }
    }
}
