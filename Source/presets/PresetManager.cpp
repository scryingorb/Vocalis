#include "PresetManager.h"
#include "../ParameterIDs.h"
#include <cmath>

const std::array<const char*, PresetManager::parameterCount>& PresetManager::parameterIDs() noexcept
{
    using namespace ParameterIDs;
    static const std::array<const char*, parameterCount> ids{inputGain,dryWet,outputGain,bypass,pitch,
        formantShift,formantPreserve,correctionAmount,retuneSpeed,noteTransition,hardTune,
        humanize,vibratoPreserve,pitchDrift,transitionSmoothness,bloom};
    return ids;
}
const std::array<PresetManager::Preset, PresetManager::factoryCount>& PresetManager::factory() noexcept
{
    // In, wet, out, bypass, pitch, formant, preserve, correction, speed,
    // transition, hard, humanize, vibrato, drift, smoothness, Bloom.
    // Key/scale are musical context and are intentionally retained on recall.
    static const std::array<Preset, factoryCount> presets{{
        {"Natural",  {0,100, 0,0, 0, 0,100, 65,40,55,  0,65,70,10,55,15}},
        {"Pop",      {0,100, 0,0, 0, 0,100, 95,75,25, 55,25,35, 5,25,25}},
        {"Hard Tune",{0,100, 0,0, 0, 0,100,100,100,0,100, 0, 0, 0, 0, 0}},
        {"Feminine", {0,100,-1,0, 3, 1.75f,100,75,55,40,20,50,55,10,50,30}},
        {"Deep",     {0,100,-1,0,-3,-1.75f,100,75,50,40,20,45,45, 5,50,10}},
        {"Airy",     {0,100,-1,0, 0, .75f,100,55,35,60, 0,60,70,15,65,75}},
        {"Robot",    {0,100,-1,0, 0,-.75f,100,100,100,0,100,0,0,0,0,0}},
        {"Cartoon",  {0,100,-3,0, 7, 3,100,80,80,15,65,10,10,0,15,15}},
        {"Monster",  {0,100,-3,0,-9,-4,100,85,65,25,65,10,10,0,25,0}}
    }};
    return presets;
}
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state)
{
    for (size_t i=0;i<parameterCount;++i)
    {
        parameters[i]=state.getParameter(parameterIDs()[i]);
        values[i]=state.getRawParameterValue(parameterIDs()[i]);
        jassert(parameters[i] != nullptr && values[i] != nullptr);
    }
}
bool PresetManager::apply(size_t index)
{
    if (index>=factoryCount) return false;
    const auto& preset=factory()[index];
    for (size_t i=0;i<parameterCount;++i)
    {
        auto& parameter=*parameters[i];
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost(parameter.convertTo0to1(preset.values[i]));
        parameter.endChangeGesture();
    }
    return true;
}
int PresetManager::activeIndex() const noexcept
{
    for (size_t p=0;p<factoryCount;++p)
    {
        bool match=true;
        for(size_t i=0;i<parameterCount;++i)
            if(std::abs(values[i]->load(std::memory_order_relaxed)-factory()[p].values[i])>0.002f)
            {match=false;break;}
        if(match) return static_cast<int>(p);
    }
    return -1;
}
