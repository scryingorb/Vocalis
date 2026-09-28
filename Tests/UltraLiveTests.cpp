#include "../Source/dsp/live/UltraLiveEngine.h"
#include "../Source/dsp/PitchShifter.h"
#include "../Source/dsp/BloomMapping.h"
#include "../Source/presets/PresetManager.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

// Allocation interceptor defined in PitchTests.cpp; accepts the research engine
// without routing it through the released processor.
void processUltraWithoutAllocations(UltraLiveEngine&,const float* const*,float* const*,int);
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void detectorMeasurements()
{
    std::ofstream file("phase11-detector.csv");file<<"profile,rate,hz,first_detection_ms,steady_error_cents,wrong_or_unvoiced_frames,steady_frames\n";
    for(auto profile:{PitchDetector::Profile::balanced,PitchDetector::Profile::fastExperimental})
    for(double rate:{44100.,48000.,88200.,96000.})for(double hz:{65.,80.,110.,220.,440.,880.})
    {
        PitchDetector detector;detector.prepare(rate,profile);int first=-1,wrong=0,frames=0;double error=0;
        for(int i=0;i<int(rate*.6);++i)
        {
            const double phase=juce::MathConstants<double>::twoPi*hz*i/rate;
            const float x=float(.06*std::sin(phase)+.03*std::sin(phase*2)+.015*std::sin(phase*3));
            if(detector.processSample(x))
            {
                const auto result=detector.result();const double cents=result.hz>0?1200*std::log2(result.hz/hz):9999;
                if(first<0&&std::abs(cents)<20)first=i;
                if(i>rate*.2){++frames;wrong+=std::abs(cents)>20;error+=std::abs(cents);}
            }
        }
        file<<(profile==PitchDetector::Profile::balanced?"balanced":"fast")<<','<<rate<<','<<hz<<','<<(first<0?-1:first*1000/rate)<<','<<error/frames<<','<<wrong<<','<<frames<<'\n';
    }
}
void responseMeasurements()
{
    std::ofstream file("phase11-response.csv");file<<"mode,rate,input_note_detected_ms,corrector_target_ms,output_settled_ms,output_minus_target_ms\n";
    for(double rate:{44100.,48000.,88200.,96000.})for(int mode:{0,1,2,3})
    {
        PitchCorrector controls;controls.prepare(rate,mode>=2?PitchDetector::Profile::fastExperimental:PitchDetector::Profile::balanced);
        controls.setControls({0,2,100,100,0,100});
        PitchShifter spectral;UltraLiveEngine ultra;
        if(mode>=2){ultra.prepare(rate,1,0,0,100,mode==2?30:20);ultra.setCorrectionControls({0,2,100,100,0,100});}
        else {spectral.prepare(rate,1,0,mode==0?PitchShifter::Mode::live:PitchShifter::Mode::quality);spectral.setCorrectionControls({0,2,100,100,0,100});}
        PitchDetector observer;observer.prepare(rate,PitchDetector::Profile::fastExperimental);
        const int at=int(rate*.35);int detected=-1,target=-1,settled=-1,consecutive=0;double phase=0;
        for(int i=0;i<int(rate*.9);++i)
        {
            const double hz=i<at?201:239;phase+=juce::MathConstants<double>::twoPi*hz/rate;
            float input=float(.08*std::sin(phase)+.02*std::sin(2*phase)),output=0;
            controls.processSample(input);
            if(i>=at&&detected<0&&std::abs(controls.detectedHz()-239)<2)detected=i;
            if(i>=at&&target<0&&controls.targetNote()==58)target=i;
            const float* in[]={&input};float* out[]={&output};if(mode>=2)ultra.process(in,out,1);else spectral.process(in,out,1);
            if(observer.processSample(output)&&i>=at)
            {
                const float f=observer.result().hz;const bool near=f>0&&std::abs(1200*std::log2(f/233.08188))<20;
                consecutive=near?consecutive+1:0;if(consecutive>=3&&settled<0)settled=i;
            }
        }
        const auto ms=[&](int i){return i<0?-1:(i-at)*1000/rate;};
        file<<(mode==0?"LIVE":mode==1?"QUALITY":mode==2?"ULTRA30":"ULTRA20")<<','<<rate<<','<<ms(detected)<<','<<ms(target)<<','<<ms(settled)<<','<<(settled>=0&&target>=0?(settled-target)*1000/rate:-1)<<'\n';
    }
}
void longAutomationTest()
{
    UltraLiveEngine engine;
    std::vector<float> input(512),a(512),b(512);
    const float* in[]={input.data(),input.data()};float* out[]={a.data(),b.data()};
    double maximum=0;uint64_t totalGrains=0;
    // Two minutes of simulated streaming, including stopped/reprepared mode
    // changes. No callback may reconfigure or allocate storage.
    for(double delay:{30.,20.,30.,20.})
    {
        engine.prepare(48000,2,0,0,100,delay);engine.setCorrectionControls({0,2,100,100,0,100});
        for(int block=0;block<2813;++block)
        {
            if(block%100==0)
            {
                engine.setSemitones(float(block%25)-12);
                const auto character=BloomMapping::apply(float(block%101),{50,40,20,60},float(block%13)-6);
                engine.setFormants(character.formant,float(block%101));engine.setHumanizeControls(character.humanizer);
                engine.setCorrectionControls({block%12,block%3,100,float(block%101),20,float(block%101)});
            }
            for(int i=0;i<512;++i)input[size_t(i)]=float(.04*std::sin((block*512+i)*juce::MathConstants<double>::twoPi*220/48000));
            processUltraWithoutAllocations(engine,in,out,512);
            for(int i=0;i<512;++i){check(std::isfinite(a[size_t(i)]),"Long session nonfinite");check(a[size_t(i)]==b[size_t(i)],"Long session stereo divergence");maximum=std::max(maximum,double(std::abs(a[size_t(i)])));}
        }
        totalGrains+=engine.grainCount();
    }
    check(maximum<1,"Long session runaway gain");
    std::cout<<"ULTRA 120-second simulated automation/reprepare: peak="<<maximum<<" grains="<<totalGrains<<"\n";
}void integrityTests()
{
    for(double rate:{44100.,48000.,88200.,96000.})for(int channels:{1,2})for(double delay:{20.,30.})
    {
        UltraLiveEngine e;e.prepare(rate,channels,0,0,100,delay);
        const int n=int(rate*.4);std::vector<float> input(size_t(n),0),a(size_t(n),0),b(a);
        input[0]=.5f;const float* in[]={input.data(),input.data()};float* out[]={a.data(),b.data()};
        processUltraWithoutAllocations(e,in,out,n);
        for(int i=0;i<n;++i)check(a[size_t(i)]==(i==e.latencySamples()?.5f:0.f),"ULTRA neutral delay mismatch");
        for(size_t preset=0;preset<PresetManager::factoryCount;++preset)
        {
            // Factory order is unchanged: gain/mix/bypass belong to the processor,
            // remaining values feed the same shared macro and note logic here.
            const auto& v=PresetManager::factory()[preset].values;
            auto character=BloomMapping::apply(v[15],{v[11],v[12],v[13],v[14]},v[5]);
            e.setSemitones(v[4]);e.setFormants(character.formant,v[6]);
            e.setCorrectionControls({0,2,v[7],v[8],v[9],v[10]});e.setHumanizeControls(character.humanizer);e.reset();
            for(int i=0;i<n;++i)input[size_t(i)]=float(.06*std::sin(juce::MathConstants<double>::twoPi*220*i/rate));
            input[13]=std::numeric_limits<float>::infinity();input[31]=std::numeric_limits<float>::quiet_NaN();
            int block=0;const int sizes[]={1,17,32,64,128,256,511};
            for(int offset=0;offset<n;)
            {
                const int count=std::min(sizes[block++%7],n-offset);const float* src[]={input.data()+offset,input.data()+offset};float* dst[]={a.data()+offset,b.data()+offset};
                processUltraWithoutAllocations(e,src,dst,count);offset+=count;
            }
            for(int i=0;i<n;++i){check(std::isfinite(a[size_t(i)])&&std::abs(a[size_t(i)])<1,"ULTRA nonfinite/runaway");if(channels==2)check(a[size_t(i)]==b[size_t(i)],"ULTRA channel clock mismatch");}
        }
        e.reset();std::fill(input.begin(),input.end(),0.0f);processUltraWithoutAllocations(e,in,out,n);
        for(float x:a)check(x==0,"ULTRA reset leaked history");
    }
}
}
void runUltraLiveTests()
{
    integrityTests();longAutomationTest();detectorMeasurements();responseMeasurements();
    std::cout<<"PASS: isolated ULTRA memory/finite/stereo/reset/neutral-delay checks; detector and response measurements written (quality acceptance is separate)\n";
}
