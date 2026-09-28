#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class PresetManager final
{
public:
    static constexpr size_t parameterCount = 16, factoryCount = 9;
    struct Preset { const char* name; std::array<float, parameterCount> values; };
    explicit PresetManager(juce::AudioProcessorValueTreeState&);
    // Message-thread UI action: normal parameter notifications, no DSP reset.
    bool apply(size_t index);
    int activeIndex() const noexcept; // -1 = Custom; no name metadata to go stale
    static const std::array<Preset, factoryCount>& factory() noexcept;
    static const std::array<const char*, parameterCount>& parameterIDs() noexcept;
private:
    std::array<juce::RangedAudioParameter*, parameterCount> parameters{};
    std::array<std::atomic<float>*, parameterCount> values{};
};
