#include "../Source/PluginProcessor.h"
#include "../Source/dsp/PitchDetector.h"
#include "../Source/dsp/PitchCorrector.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void processDspWithoutAllocations(PitchShifter&, const float* const*, float* const*, int);
void processWithoutAllocations(juce::AudioProcessor&, juce::AudioBuffer<float>&, juce::MidiBuffer&, bool);
namespace {
constexpr double tau = 6.283185307179586;
void check(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
void set(VocalisAudioProcessor& p, const char* id, float v)
{
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
}
double frequency(const std::vector<float>& audio, double rate)
{
    double first = 0, last = 0; int count = 0;
    for (int i = static_cast<int>(audio.size() / 2); i < static_cast<int>(audio.size()); ++i)
        if (audio[static_cast<size_t>(i-1)] <= 0 && audio[static_cast<size_t>(i)] > 0)
        {
            last = i - 1 - audio[static_cast<size_t>(i-1)] / double(audio[static_cast<size_t>(i)] - audio[static_cast<size_t>(i-1)]);
            if (count++ == 0) first = last;
        }
    return count > 2 ? (count - 1) * rate / (last - first) : 0;
}
void detectorRangeTests()
{
    // Exercise every steady update: testing only the final frame misses
    // sample-rate-dependent dropouts at the inclusive detector endpoints.
    for (double rate : {44100., 48000., 88200., 96000.})
        for (auto profile : {PitchDetector::Profile::balanced, PitchDetector::Profile::fastExperimental})
            for (double hz : {64., 65., 65.1, 1099., 1100., 1110.})
                for (bool harmonic : {false, true})
                {
                    PitchDetector detector; detector.prepare(rate, profile);
                    int updates = 0;
                    for (int i = 0; i < static_cast<int>(rate * .4); ++i)
                    {
                        const double phase = tau * hz * i / rate;
                        const float sample = static_cast<float>(.1 * (std::sin(phase)
                            + (harmonic ? .3 * std::sin(2 * phase) : 0)));
                        if (!detector.processSample(sample) || i < static_cast<int>(rate * .2)) continue;
                        ++updates;
                        const auto result = detector.result();
                        if (hz < 65 || hz > 1100)
                            check(result.hz == 0, "Detector accepted a tone beyond its boundary tolerance");
                        else
                        {
                            check(result.confidence > .85f && result.hz >= 65 && result.hz <= 1100,
                                  "Detector dropped a supported range endpoint");
                            check(std::abs(1200 * std::log2(result.hz / hz)) < 10,
                                  "Detector range endpoint pitch error");
                        }
                    }
                    check(updates > 20, "Insufficient detector boundary observations");
                }
    std::cout << "PASS: detector endpoints, adjacent pitches and outside-range rejection in both profiles\n";
}
void detectorTests()
{
    detectorRangeTests();
    for (double rate : {44100.,48000.,96000.})
        for (double hz : {80.,110.,150.,220.})
            for (double second : {0.,1.,4.,8.})
            {
                PitchDetector detector;detector.prepare(rate);
                int voiced=0,updates=0;
                for(int i=0;i<int(rate*.6);++i)
                {
                    const double phase=tau*hz*i/rate;
                    const float x=float(.1/(1+second)*(std::sin(phase)+second*std::sin(2*phase)));
                    if(!detector.processSample(x)||i<int(rate*.2))continue;
                    ++updates;
                    const auto result=detector.result();
                    if(result.hz>0&&std::abs(1200*std::log2(result.hz/hz))<10)++voiced;
                }
                check(voiced==updates&&updates>20,"Dominant harmonic caused octave error or dropout");
            }
    // Breathy voiced input, consonant-like noise and genuine octave transitions:
    // periodicity improvements must not create voiced noise or hold the old octave.
    for(double rate:{44100.,48000.,96000.})
    {
        PitchDetector detector;detector.prepare(rate);juce::Random random(712);
        for(double hz:{220.,440.,0.,110.})
        {
            int observed=0,correct=0;
            for(int i=0;i<int(rate*.4);++i)
            {
                const float noise=(random.nextFloat()-.5f)*float(hz>0?.06:.2);
                const float sample=noise+float(hz>0?.1*std::sin(tau*hz*i/rate):0);
                if(!detector.processSample(sample)||i<int(rate*.2))continue;
                ++observed;
                const auto result=detector.result();
                if(hz==0?result.hz==0:(result.hz>0&&std::abs(1200*std::log2(result.hz/hz))<15))++correct;
            }
            check(observed>20&&correct>=observed*.95,"Breathy/noise/octave-transition tracking regression");
        }
    }
    std::cout<<"PASS: dominant harmonics, breathy fixtures, unvoiced noise and genuine octave transitions\n";
    double worst = 0;
    for (double rate : {44100., 48000., 96000.})
        for (double hz : {75., 110., 220., 440., 880.})
            for (bool missingFundamental : {false, true})
            {
                PitchDetector d; d.prepare(rate);
                for (int i = 0; i < static_cast<int>(rate * .6); ++i)
                {
                    double phase = tau * hz * i / rate;
                    float x = static_cast<float>(.1 * (missingFundamental ? std::sin(phase*2) + .7*std::sin(phase*3) : std::sin(phase) + .3*std::sin(phase*2)));
                    d.processSample(x);
                }
                const auto result = d.result();
                const double cents = result.hz > 0 ? 1200 * std::log2(result.hz / hz) : 9999;
                worst = std::max(worst, std::abs(cents));
                if (std::abs(cents) > 10) std::cout << "Detector " << rate << ' ' << hz << " missing=" << missingFundamental << " measured=" << result.hz << " cents=" << cents << std::endl;
                check(result.confidence > .85f && std::abs(cents) < 10, "Vocal F0 tracking failed");
                for (int i=0;i<static_cast<int>(rate*.2);++i) d.processSample(0);
                check(d.result().hz == 0, "Silence retained detected note");
            }
    PitchDetector d; d.prepare(48000); juce::Random random(1234); int voiced=0, updates=0;
    for (int i=0;i<96000;++i) if(d.processSample((random.nextFloat()-.5f)*.2f)) { ++updates; if(d.result().hz>0) ++voiced; }
    check(voiced < updates / 20, "Noise produced excessive false pitch detections");
    std::cout << "Detector maximum error " << worst << " cents; noise voiced frames " << voiced << '/' << updates << '\n';
}
void controlTests()
{
    for (int key=0;key<12;++key) for (int scale=0;scale<3;++scale)
        for (float note=36;note<85;note+=.25f)
        {
            int target=PitchCorrector::nearestNote(note,key,scale);
            check(PitchCorrector::allowed(target,key,scale),"Scale target is forbidden");
            for(int n=30;n<96;++n) if(PitchCorrector::allowed(n,key,scale)) check(std::abs(note-target)<=std::abs(note-n)+1e-6,"Scale target not nearest");
        }
    check(PitchCorrector::allowed(60,0,0)&&!PitchCorrector::allowed(61,0,0),"C major intervals");
    check(PitchCorrector::allowed(60,9,1)&&!PitchCorrector::allowed(61,9,1),"A natural minor intervals");
    double retained[2]{};
    for (int hard=0;hard<=1;++hard)
    {
        PitchCorrector c; c.prepare(48000); c.setControls({0,2,100,100,50,float(hard*100)});
        double phase=0, low=1e9, high=-1e9;
        for(int i=0;i<144000;++i)
        {
            double note=57.25+.22*std::sin(tau*5*i/48000.0);
            phase += tau*440*std::pow(2.,(note-69)/12)/48000;
            float shift=c.processSample(float(.1*std::sin(phase)));
            if(i>48000) { low=std::min(low,note+shift); high=std::max(high,note+shift); }
        }
        retained[hard]=high-low;
    }
    std::cout<<"Vibrato control-domain span natural/hard: "<<retained[0]<<" / "<<retained[1]<<" st\n";
    check(retained[0]>.3 && retained[1]<retained[0]*.85,"Natural/Hard vibrato behavior");
    int transitionSamples[2]{};
    for (int slow=0;slow<2;++slow)
    {
        PitchCorrector c; c.prepare(48000); c.setControls({0,2,100,100,float(slow*100),0});
        double phase=0;
        for(int i=0;i<96000;++i)
        {
            const double hz=i<48000?220:246.94165;
            phase+=tau*hz/48000;
            c.processSample(float(.1*std::sin(phase)));
            if(i>=48000 && c.targetNote()==59 && transitionSamples[slow]==0) transitionSamples[slow]=i-48000;
        }
    }
    check(transitionSamples[1]>transitionSamples[0]+2500,"Note Transition failed to hold target");
    std::cout<<"Note transition delays low/high: "<<transitionSamples[0]/48.0<<" / "<<transitionSamples[1]/48.0<<" ms\n";
    float corrections[2]{};
    for(int fast=0;fast<2;++fast)
    {
        PitchCorrector c;c.prepare(48000);c.setControls({0,2,100,float(fast*100),0,100});
        for(int i=0;i<7200;++i) corrections[fast]=c.processSample(float(.1*std::sin(tau*225*i/48000)));
    }
    check(std::abs(corrections[1])>std::abs(corrections[0])*1.5,"Retune Speed did not affect response");
}
std::vector<float> render(double rate, int channels, float amount, int key, int scale, double inputHz, float manual=0, bool irregular=true)
{
    VocalisAudioProcessor p; p.setPlayConfigDetails(channels,channels,rate,64);
    set(p,ParameterIDs::correctionAmount,amount);set(p,ParameterIDs::retuneSpeed,100);
    set(p,ParameterIDs::hardTune,100);set(p,ParameterIDs::noteTransition,0);
    set(p,ParameterIDs::key,float(key));set(p,ParameterIDs::scale,float(scale));
    set(p,ParameterIDs::formantPreserve,0); // pure-tone pitch oracle, without envelope attenuation
    set(p,ParameterIDs::pitch,manual);p.prepareToPlay(rate,64);
    check(p.getLatencySamples()==int(rate*.048)+(rate>=88200?int(rate*.003):0)+64,"Correction changed reported delay");
    const int length=int(rate*1.4);std::vector<float> out(static_cast<size_t>(length));
    juce::AudioBuffer<float> buffer(channels,1024);juce::MidiBuffer midi;
    int block=0;const int sizes[]={1,17,64,127,511,1024};
    for(int pos=0;pos<length;)
    {
        int n=std::min(irregular?sizes[block++%6]:64,length-pos);buffer.setSize(channels,n,false,false,true);
        for(int i=0;i<n;++i) for(int ch=0;ch<channels;++ch) buffer.setSample(ch,i,float(.1*std::sin(tau*inputHz*(pos+i)/rate)));
        processWithoutAllocations(p,buffer,midi,false);
        for(int i=0;i<n;++i) {out[static_cast<size_t>(pos+i)]=buffer.getSample(0,i);check(std::isfinite(out[static_cast<size_t>(pos+i)])&&std::abs(out[static_cast<size_t>(pos+i)])<1,"Correction output unstable");if(channels==2)check(buffer.getSample(0,i)==buffer.getSample(1,i),"Correction stereo desync");}
        pos+=n;
    }
    return out;
}
void renderedVibratoTest()
{
    double spans[2]{};
    for(int hard=0;hard<2;++hard)
    {
        VocalisAudioProcessor p;set(p,ParameterIDs::correctionAmount,100);set(p,ParameterIDs::retuneSpeed,100);
        set(p,ParameterIDs::hardTune,float(hard*100));set(p,ParameterIDs::formantPreserve,0);p.prepareToPlay(48000,256);
        juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;std::vector<float> audio;audio.reserve(144128);double phase=0;
        for(int block=0;block<563;++block)
        {
            for(int i=0;i<256;++i) {double note=57.25+.22*std::sin(tau*5*(block*256+i)/48000.);phase+=tau*440*std::pow(2.,(note-69)/12)/48000;float x=float(.1*std::sin(phase));buffer.setSample(0,i,x);buffer.setSample(1,i,x);}
            processWithoutAllocations(p,buffer,midi,false);audio.insert(audio.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
        }
        std::vector<double> notes;double previous=0;
        for(int i=48001;i<int(audio.size());++i)if(audio[size_t(i-1)]<=0&&audio[size_t(i)]>0)
        {
            double crossing=i-1-audio[size_t(i-1)]/double(audio[size_t(i)]-audio[size_t(i-1)]);
            if(previous>0)notes.push_back(69+12*std::log2(48000/(crossing-previous)/440));previous=crossing;
        }
        std::sort(notes.begin(),notes.end());spans[hard]=notes[notes.size()*95/100]-notes[notes.size()*5/100];
    }
    std::cout<<"Rendered 5 Hz vibrato 5th-95th percentile span natural/hard: "<<spans[0]<<" / "<<spans[1]<<" st\n";
    check(spans[0]>.25&&spans[1]<spans[0]*.8,"Rendered vibrato preservation/hardening failed");
}

void automationTests()
{
    for(double rate:{44100.,48000.,96000.}) for(int channels:{1,2})
    {
        VocalisAudioProcessor p; p.setPlayConfigDetails(channels,channels,rate,64);p.prepareToPlay(rate,64);
        juce::AudioBuffer<float> buffer(channels,511);juce::MidiBuffer midi;double phase=0;float previous=0;double maxStep=0;
        for(int block=0;block<1000;++block)
        {
            if(block%29==0 && block<500)
            {
                set(p,ParameterIDs::correctionAmount,float(block%101));set(p,ParameterIDs::retuneSpeed,float((block*7)%101));
                set(p,ParameterIDs::hardTune,float((block*13)%101));set(p,ParameterIDs::noteTransition,float((block*17)%101));
                set(p,ParameterIDs::key,float(block%12));set(p,ParameterIDs::scale,float(block%3));
                set(p,ParameterIDs::formantShift,float(block%7-3));
            }
            if(block==500){set(p,ParameterIDs::correctionAmount,0);set(p,ParameterIDs::formantShift,0);}
            for(int i=0;i<511;++i) {float x=float(.04*std::sin(phase));phase+=tau*225/rate;for(int ch=0;ch<channels;++ch)buffer.setSample(ch,i,x);}
            processWithoutAllocations(p,buffer,midi,false);
            for(int i=0;i<511;++i){float x=buffer.getSample(0,i);check(std::isfinite(x)&&std::abs(x)<1,"Correction automation instability");maxStep=std::max(maxStep,double(std::abs(x-previous)));previous=x;}
        }
        check(maxStep<.03,"Correction automation caused large discontinuity");
        check(p.detectedPitchHz.load()==0,"Disabled detector retained UI pitch");
    }
}
void qualityModeTests()
{
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})
    {
        PitchShifter dsp;dsp.prepare(rate,channels,0,PitchShifter::Mode::quality,0,0);
        dsp.setCorrectionControls({0,2,100,100,0,100});
        std::vector<float> input(size_t(rate),0),left(input.size()),right(input.size());
        for(size_t i=0;i<input.size();++i)input[i]=float(.1*std::sin(tau*225*i/rate));
        const float* in[]={input.data(),input.data()};float* out[]={left.data(),right.data()};
        processDspWithoutAllocations(dsp,in,out,int(input.size()));
        check(std::abs(1200*std::log2(frequency(left,rate)/220))<8,"QUALITY correction failed");
        check(dsp.latencySamples()==int(rate*.12)+64,"QUALITY correction latency changed");
    }
}

void correctionBenchmark()
{
    std::ofstream csv("correction-cpu.csv");csv<<"rate,buffer,amount,cpu_percent,p99_us,max_us,deadline_misses\n";
    for(double rate:{44100.,48000.,96000.})for(int size:{64,256})for(float amount:{0.f,100.f})
    {
        VocalisAudioProcessor p;p.setPlayConfigDetails(2,2,rate,size);
        set(p,ParameterIDs::correctionAmount,amount);set(p,ParameterIDs::hardTune,100);set(p,ParameterIDs::retuneSpeed,100);
        p.prepareToPlay(rate,size);juce::AudioBuffer<float> buffer(2,size);juce::MidiBuffer midi;
        std::vector<float> source(size_t(rate*3),0);for(size_t i=0;i<source.size();++i)source[i]=float(.08*std::sin(tau*225*i/rate)+.03*std::sin(tau*450*i/rate));
        std::vector<double> times;times.reserve(source.size()/size+1);double sum=0;int misses=0;
        for(int pos=0;pos<int(source.size());pos+=size)
        {
            int n=std::min(size,int(source.size())-pos);buffer.setSize(2,n,false,false,true);
            for(int ch=0;ch<2;++ch)std::copy_n(source.data()+pos,n,buffer.getWritePointer(ch));
            auto start=std::chrono::steady_clock::now();p.processBlock(buffer,midi);
            double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();times.push_back(us);sum+=us;if(us>n*1e6/rate)++misses;
        }
        std::sort(times.begin(),times.end());csv<<rate<<','<<size<<','<<amount<<','<<sum/30000<<','<<times[size_t(times.size()*.99)]<<','<<times.back()<<','<<misses<<'\n';
    }
}

void stateTests()
{
    VocalisAudioProcessor p;
    const char* ids[]={ParameterIDs::key,ParameterIDs::scale,ParameterIDs::correctionAmount,ParameterIDs::retuneSpeed,ParameterIDs::noteTransition,ParameterIDs::hardTune};
    const float values[]={11,1,72.5f,83,24,65};
    for(int i=0;i<6;++i)set(p,ids[i],values[i]);
    juce::MemoryBlock state;p.getStateInformation(state);VocalisAudioProcessor restored;
    restored.setStateInformation(state.getData(),int(state.getSize()));
    for(int i=0;i<6;++i)check(std::abs(restored.parameters.getRawParameterValue(ids[i])->load()-values[i])<.001f,"Correction state round-trip");
    auto legacy=p.parameters.copyState();
    for(int i=legacy.getNumChildren()-1;i>=0;--i)for(auto id:ids)if(legacy.getChild(i).getProperty("id").toString()==id){legacy.removeChild(i,nullptr);break;}
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),state);restored.setStateInformation(state.getData(),int(state.getSize()));
    check(restored.parameters.getRawParameterValue(ParameterIDs::correctionAmount)->load()==0,"Legacy state enabled correction");
}
}
void runCorrectionTests()
{
    detectorTests();controlTests();stateTests();automationTests();renderedVibratoTest();qualityModeTests();
    std::ofstream csv("correction-pitch.csv");csv<<"rate,channels,amount,key,scale,input_hz,expected_hz,measured_hz,error_cents\n";
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})
    {
        for(float amount:{0.f,50.f,100.f})
        {
            auto audio=render(rate,channels,amount,0,2,225);
            double expected=225*std::pow(220./225,amount*.01);
            double measured=frequency(audio,rate),cents=1200*std::log2(measured/expected);
            csv<<rate<<','<<channels<<','<<amount<<",0,2,225,"<<expected<<','<<measured<<','<<cents<<'\n';
            if(std::abs(cents)>10)std::cout<<"Correction error "<<rate<<" "<<amount<<" "<<cents<<std::endl;
            check(std::abs(cents)<10,"Rendered correction pitch error");
            if(amount==100)check(audio==render(rate,channels,amount,0,2,225,0,false),"Correction depends on host buffer partition");
        }
        for(int scale:{0,1})
        {
            int key=scale==0?0:9;double input=scale==0?187:210;
            double target=scale==0?196:220;
            auto audio=render(rate,channels,100,key,scale,input);
            double measured=frequency(audio,rate),cents=1200*std::log2(measured/target);
            csv<<rate<<','<<channels<<",100,"<<key<<','<<scale<<','<<input<<','<<target<<','<<measured<<','<<cents<<'\n';
            check(std::abs(cents)<15,"Key/scale correction audio failed");
        }
        auto shifted=render(rate,channels,100,0,2,225,3);
        check(std::abs(1200*std::log2(frequency(shifted,rate)/(220*std::pow(2.,.25))))<15,"Manual plus automatic pitch failed");
    }
    correctionBenchmark();
    std::cout<<"PASS: correction detector, scales, vibrato, retune, state, rendered pitch, buffers and allocations\n";
}
