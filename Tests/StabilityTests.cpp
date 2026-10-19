#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

void processWithoutAllocations(juce::AudioProcessor&, juce::AudioBuffer<float>&, juce::MidiBuffer&, bool);
namespace {
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void set(VocalisAudioProcessor& p, const char* id, float value)
{
    auto* parameter=p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void inspect(const juce::AudioBuffer<float>& b, float& peak, float& jump, float* previous)
{
    for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i)
    {
        const float x=b.getSample(ch,i);
        require(std::isfinite(x),"Non-finite output in stability test");
        peak=std::max(peak,std::abs(x));jump=std::max(jump,std::abs(x-previous[ch]));previous[ch]=x;
    }
}
void stateValidation()
{
    VocalisAudioProcessor p;
    auto state=p.parameters.copyState();
    for(auto child:state)
    {
        const auto id=child.getProperty("id").toString();
        if(id==ParameterIDs::pitch)child.setProperty("value",std::numeric_limits<double>::infinity(),nullptr);
        if(id==ParameterIDs::formantShift)child.setProperty("value",999.0,nullptr);
    }
    juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(*state.createXml(),data);
    p.setStateInformation(data.getData(),int(data.getSize()));
    for(auto* parameter:p.getParameters()) require(std::isfinite(parameter->getValue())&&parameter->getValue()>=0&&parameter->getValue()<=1,"Invalid restored parameter");
    require(p.parameters.getRawParameterValue(ParameterIDs::formantShift)->load()==6,"Out-of-range state not clamped");

    const auto restore = [](VocalisAudioProcessor& processor, const juce::ValueTree& tree)
    {
        juce::MemoryBlock serialized;
        juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), serialized);
        processor.setStateInformation(serialized.getData(), static_cast<int>(serialized.getSize()));
    };
    VocalisAudioProcessor fresh, reused;
    for (auto* parameter : reused.getParameters())
        parameter->setValueNotifyingHost(parameter->getDefaultValue() < 0.5f ? 0.75f : 0.25f);

    // Missing children must not retain previous instance values. Preserve the
    // original session migration: an absent formantPreserve means no compensation.
    juce::ValueTree partial("VocalisState"), pitch("PARAM");
    pitch.setProperty("id", ParameterIDs::pitch, nullptr);
    pitch.setProperty("value", 4.5f, nullptr);
    partial.appendChild(pitch, nullptr);
    restore(fresh, partial);
    restore(reused, partial);
    for (auto* parameter : fresh.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        require(ranged != nullptr, "State test requires ranged parameters");
        float expected = ranged->convertFrom0to1(ranged->getDefaultValue());
        if (ranged->paramID == ParameterIDs::formantPreserve) expected = 0;
        if (ranged->paramID == ParameterIDs::pitch) expected = 4.5f;
        require(std::abs(fresh.parameters.getRawParameterValue(ranged->paramID)->load() - expected) < 0.001f,
                "Partial state did not restore the expected default or supplied value");
        require(std::abs(reused.parameters.getRawParameterValue(ranged->paramID)->load() - expected) < 0.001f,
                "Partial state retained a previous instance value");
    }

    // A present parameter with a missing value uses its parameter default;
    // in particular Dry/Wet stays audible and Formant Preserve defaults to 100.
    auto missingValues = fresh.parameters.copyState();
    for (auto child : missingValues) child.removeProperty("value", nullptr);
    restore(reused, missingValues);
    for (auto* parameter : reused.getParameters())
        require(std::abs(parameter->getValue() - parameter->getDefaultValue()) < 0.00001f,
                "Missing state value did not use the parameter default");

    juce::ValueTree wrongRoot("OtherPluginState");
    wrongRoot.appendChild(pitch.createCopy(), nullptr);
    restore(reused, wrongRoot);
    for (auto* parameter : reused.getParameters())
        require(std::abs(parameter->getValue() - parameter->getDefaultValue()) < 0.00001f,
                "Wrong-root state changed parameters");
}
void reprepareAndStress()
{
    VocalisAudioProcessor p; // Reuse the same instance through channel/rate changes.
    float peak=0,jump=0;
    for(double rate:{44100.,48000.,88200.,96000.,44100.})for(int channels:{1,2})
    {
        p.releaseResources();p.setPlayConfigDetails(channels,channels,rate,64);p.prepareToPlay(rate,64);
        const int expected=int(rate*.048)+(rate>=88200?int(rate*.003):0)+64;
        require(p.getLatencySamples()==expected,"Reprepare latency incorrect");
        std::cout<<"Stability rate="<<rate<<" channels="<<channels<<" latency="<<expected<<" samples / "<<expected*1000/rate<<" ms\n";
        juce::AudioBuffer<float> b(channels,4096);juce::MidiBuffer midi;
        for(size_t preset=0;preset<PresetManager::factoryCount;++preset)
        {
            p.presets.apply(preset);p.reset();float previous[2]{};int offset=0;
            const int sizes[]={0,1,17,64,128,256,511,1024,4096};
            for(int block=0;block<90;++block)
            {
                const int count=sizes[block%9];b.setSize(channels,count,false,false,true);
                for(int ch=0;ch<channels;++ch)for(int i=0;i<count;++i)
                {
                    const double phase=juce::MathConstants<double>::twoPi*(ch==0?223:337)*(offset+i)/rate;
                    b.setSample(ch,i,float(.05*std::sin(phase)+.01*std::sin(2*phase)));
                }
                if(block==20)set(p,ParameterIDs::dryWet,50);
                if(block==30)set(p,ParameterIDs::bypass,1);
                if(block==40)set(p,ParameterIDs::bypass,0);
                if(block==50){set(p,ParameterIDs::pitch,-5.5f);set(p,ParameterIDs::formantShift,-3);set(p,ParameterIDs::bloom,100);}
                if(block==60){set(p,ParameterIDs::pitch,12);set(p,ParameterIDs::formantPreserve,0);set(p,ParameterIDs::humanize,100);}
                if(block==70){set(p,ParameterIDs::pitch,-12);set(p,ParameterIDs::correctionAmount,100);set(p,ParameterIDs::retuneSpeed,100);}
                processWithoutAllocations(p,b,midi,block>=80);
                inspect(b,peak,jump,previous);offset+=count;
            }
            p.reset();b.setSize(channels,4096,false,false,true);b.clear();
            processWithoutAllocations(p,b,midi,false);require(b.getMagnitude(0,b.getNumSamples())==0,"Reset leaked old audio");
            // Poisoned upstream values and subnormal tails must not propagate.
            b.clear();b.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
            b.setSample(0,1,std::numeric_limits<float>::infinity());b.setSample(0,2,-std::numeric_limits<float>::infinity());
            b.setSample(0,3,std::numeric_limits<float>::denorm_min());
            processWithoutAllocations(p,b,midi,false);inspect(b,peak,jump,previous);
            for(int tail=0;tail<8;++tail){b.clear();processWithoutAllocations(p,b,midi,false);inspect(b,peak,jump,previous);}
        }
    }
    std::cout<<"Stability maximum absolute output="<<peak<<" maximum adjacent step="<<jump<<" (input peak <=0.06)\n";
    require(peak<.5f,"Runaway gain");require(jump<.25f,"Gross discontinuity during automation");
}
void loudInputStress()
{
    // Above-full-scale input is legal floating audio, not NaN/Inf. Verify that
    // extreme gain and transformations stay finite without adding a limiter.
    for (double rate : {44100., 48000., 88200., 96000.})
        for (int channels : {1, 2})
        {
            VocalisAudioProcessor p;
            auto layout = p.getBusesLayout();
            layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            layout.outputBuses.set(0, layout.inputBuses[0]);
            require(p.setBusesLayout(layout), "Loud test bus layout");
            set(p, ParameterIDs::inputGain, 24); set(p, ParameterIDs::outputGain, 24);
            set(p, ParameterIDs::pitch, 12); set(p, ParameterIDs::formantShift, 6);
            p.prepareToPlay(rate, 512);
            juce::AudioBuffer<float> b(channels, 512); juce::MidiBuffer midi;
            float peak = 0;
            for (int block = 0; block < 160; ++block)
            {
                if (block == 80) { set(p, ParameterIDs::pitch, -12); set(p, ParameterIDs::formantShift, -6); }
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < 512; ++i)
                        b.setSample(ch, i, float(2.0 * std::sin(juce::MathConstants<double>::twoPi * 223 * (block * 512 + i) / rate)));
                processWithoutAllocations(p, b, midi, false);
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float value = b.getSample(ch, i);
                        require(std::isfinite(value), "Loud input produced non-finite output");
                        peak = std::max(peak, std::abs(value));
                    }
            }
            require(peak > 1 && peak < 2000, "Loud input unexpectedly limited or runaway");
            p.reset(); b.clear(); processWithoutAllocations(p, b, midi, false);
            require(b.getMagnitude(0, 512) == 0, "Loud history survived reset");
        }
    std::cout << "PASS: above-full-scale input with maximum gains and extreme transformations at four rates, mono/stereo\n";
}
void concurrentControlStress()
{
    VocalisAudioProcessor p;p.prepareToPlay(48000,256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    std::atomic<bool> done{false};std::exception_ptr failure;
    std::thread audio([&] {
        try {
            juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;
            float peak=0,jump=0,previous[2]{};
            for(int block=0;block<1500;++block)
            {
                for(int i=0;i<256;++i){const float x=.05f*std::sin(float((block*256+i)*.03));b.setSample(0,i,x);b.setSample(1,i,x);}
                processWithoutAllocations(p,b,midi,false);inspect(b,peak,jump,previous);
            }
            require(peak<.5f,"Concurrent controls caused runaway gain");
        } catch (...) { failure=std::current_exception(); }
        done.store(true);
    });
    int iteration=0;
    while(!done.load())
    {
        p.presets.apply(size_t(iteration++%9));
        juce::MemoryBlock state;p.getStateInformation(state);p.setStateInformation(state.getData(),int(state.getSize()));
        p.inputPeak.exchange(0);p.outputPeak.exchange(0);p.presets.activeIndex();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    audio.join();if(failure)std::rethrow_exception(failure);
    std::cout<<"Concurrent preset/state/editor stress iterations="<<iteration<<"\n";
}
}
void runStabilityTests()
{
    stateValidation();reprepareAndStress();loudInputStress();concurrentControlStress();
    std::cout<<"PASS: rate/channel reprepare, all presets, zero/variable buffers, automation, finite output, reset, hostile input, concurrent state/UI controls and callback allocations\n";
}
