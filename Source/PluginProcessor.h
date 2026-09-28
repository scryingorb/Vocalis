#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/PitchShifter.h"
#include "ParameterIDs.h"
#include "presets/PresetManager.h"
#include <array>

class VocalisAudioProcessor final : public juce::AudioProcessor
{
public:
    VocalisAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocalis"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return pitchShifter.tailSeconds(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState parameters;
    PresetManager presets;
    std::atomic<float> detectedPitchHz{0};
    // Visual telemetry only; never fed back into processing or serialized.
    std::atomic<float> inputPeak{0}, outputPeak{0};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void render(juce::AudioBuffer<float>&, bool hostBypassed);
    std::atomic<float>* inputGain;
    std::atomic<float>* dryWet;
    std::atomic<float>* outputGain;
    std::atomic<float>* bypass;
    std::atomic<float>* pitch;
    std::atomic<float>* formantShift;
    std::atomic<float>* formantPreserve;
    std::atomic<float> *key, *scale, *correctionAmount, *retuneSpeed, *noteTransition, *hardTune;
    std::atomic<float> *humanize, *vibratoPreserve, *pitchDrift, *transitionSmoothness;
    std::atomic<float>* bloom;
    PitchShifter pitchShifter;
    juce::AudioBuffer<float> dryDelay;
    int dryPosition = 0;
    juce::SmoothedValue<float> inputSmooth, mixSmooth, outputSmooth, activeSmooth;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalisAudioProcessor)
};
