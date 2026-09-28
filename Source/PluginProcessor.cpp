#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/BloomMapping.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout VocalisAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto gainAttributes = juce::AudioParameterFloatAttributes().withLabel("dB");
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::inputGain, 1},
        "Input Gain", juce::NormalisableRange<float>{-24.0f, 24.0f, 0.01f}, 0.0f, gainAttributes));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::dryWet, 1},
        "Dry/Wet", juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::outputGain, 1},
        "Output Gain", juce::NormalisableRange<float>{-24.0f, 24.0f, 0.01f}, 0.0f, gainAttributes));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ParameterIDs::bypass, 1}, "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::pitch, 1},
        "Pitch", juce::NormalisableRange<float>{-12.0f, 12.0f, 0.01f}, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::formantShift, 1},
        "Formant Shift", juce::NormalisableRange<float>{-6.0f, 6.0f, 0.01f}, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ParameterIDs::formantPreserve, 1},
        "Formant Preserve", juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ParameterIDs::key, 1},
        "Key", juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ParameterIDs::scale, 1},
        "Scale", juce::StringArray{"Major", "Minor", "Chromatic"}, 2));
    const auto percent = [&layout](const char* id, const char* name, float value)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>{0, 100, 0.1f}, value,
            juce::AudioParameterFloatAttributes().withLabel("%")));
    };
    percent(ParameterIDs::correctionAmount, "Correction Amount", 0);
    percent(ParameterIDs::retuneSpeed, "Retune Speed", 50);
    percent(ParameterIDs::noteTransition, "Note Transition", 50);
    percent(ParameterIDs::hardTune, "Natural / Hard Tune", 0);
    percent(ParameterIDs::humanize, "Humanize", 0);
    percent(ParameterIDs::vibratoPreserve, "Vibrato Preserve", 0);
    percent(ParameterIDs::pitchDrift, "Pitch Drift", 0);
    percent(ParameterIDs::transitionSmoothness, "Transition Smoothness", 0);
    percent(ParameterIDs::bloom, "Bloom", 0);
    return layout;
}

VocalisAudioProcessor::VocalisAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "VocalisState", createParameters()),
      presets(parameters),
      inputGain(parameters.getRawParameterValue(ParameterIDs::inputGain)),
      dryWet(parameters.getRawParameterValue(ParameterIDs::dryWet)),
      outputGain(parameters.getRawParameterValue(ParameterIDs::outputGain)),
      bypass(parameters.getRawParameterValue(ParameterIDs::bypass)),
      pitch(parameters.getRawParameterValue(ParameterIDs::pitch)),
      formantShift(parameters.getRawParameterValue(ParameterIDs::formantShift)),
      formantPreserve(parameters.getRawParameterValue(ParameterIDs::formantPreserve)),
      key(parameters.getRawParameterValue(ParameterIDs::key)),
      scale(parameters.getRawParameterValue(ParameterIDs::scale)),
      correctionAmount(parameters.getRawParameterValue(ParameterIDs::correctionAmount)),
      retuneSpeed(parameters.getRawParameterValue(ParameterIDs::retuneSpeed)),
      noteTransition(parameters.getRawParameterValue(ParameterIDs::noteTransition)),
      hardTune(parameters.getRawParameterValue(ParameterIDs::hardTune)),
      humanize(parameters.getRawParameterValue(ParameterIDs::humanize)),
      vibratoPreserve(parameters.getRawParameterValue(ParameterIDs::vibratoPreserve)),
      pitchDrift(parameters.getRawParameterValue(ParameterIDs::pitchDrift)),
      transitionSmoothness(parameters.getRawParameterValue(ParameterIDs::transitionSmoothness)),
      bloom(parameters.getRawParameterValue(ParameterIDs::bloom))
{
    static_assert(std::atomic<float>::is_always_lock_free, "Audio parameters must be lock-free");
}

bool VocalisAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto channels = layout.getMainOutputChannelSet();
    return (channels == juce::AudioChannelSet::mono() || channels == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == channels;
}

void VocalisAudioProcessor::prepareToPlay(double sampleRate, int)
{
    const auto character = BloomMapping::apply(bloom->load(),
        {humanize->load(), vibratoPreserve->load(), pitchDrift->load(), transitionSmoothness->load()}, formantShift->load());
    pitchShifter.setHumanizeControls(character.humanizer);
    pitchShifter.prepare(sampleRate, getTotalNumInputChannels(), pitch->load(), PitchShifter::Mode::live,
                         character.formant, formantPreserve->load());
    setLatencySamples(pitchShifter.latencySamples());
    dryDelay.setSize(getTotalNumInputChannels(), getLatencySamples());
    dryDelay.clear();
    dryPosition = 0;
    for (auto* smoother : {&inputSmooth, &mixSmooth, &outputSmooth, &activeSmooth})
        smoother->reset(sampleRate, 0.02);
    reset();
}

void VocalisAudioProcessor::reset()
{
    const auto character = BloomMapping::apply(bloom->load(),
        {humanize->load(), vibratoPreserve->load(), pitchDrift->load(), transitionSmoothness->load()}, formantShift->load());
    pitchShifter.setHumanizeControls(character.humanizer);
    pitchShifter.reset(pitch->load(), character.formant, formantPreserve->load());
    detectedPitchHz.store(0, std::memory_order_relaxed);
    inputPeak.store(0, std::memory_order_relaxed);
    outputPeak.store(0, std::memory_order_relaxed);
    dryDelay.clear();
    dryPosition = 0;
    inputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGain->load()));
    mixSmooth.setCurrentAndTargetValue(dryWet->load() * 0.01f);
    outputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
    activeSmooth.setCurrentAndTargetValue(bypass->load() >= 0.5f ? 0.0f : 1.0f);
}

void VocalisAudioProcessor::render(juce::AudioBuffer<float>& buffer, bool hostBypassed)
{
    juce::ScopedNoDenormals noDenormals;
    float inputBlockPeak = 0, outputBlockPeak = 0;
    inputSmooth.setTargetValue(juce::Decibels::decibelsToGain(inputGain->load()));
    mixSmooth.setTargetValue(dryWet->load() * 0.01f);
    outputSmooth.setTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
    activeSmooth.setTargetValue(hostBypassed || bypass->load() >= 0.5f ? 0.0f : 1.0f);

    const auto character = BloomMapping::apply(bloom->load(),
        {humanize->load(), vibratoPreserve->load(), pitchDrift->load(), transitionSmoothness->load()}, formantShift->load());
    pitchShifter.setHumanizeControls(character.humanizer);
    pitchShifter.setCorrectionControls({static_cast<int>(key->load()), static_cast<int>(scale->load()),
        correctionAmount->load(), retuneSpeed->load(), noteTransition->load(), hardTune->load()});
    pitchShifter.setSemitones(pitch->load());
    pitchShifter.setFormants(character.formant, formantPreserve->load());
    // Bounded stack scratch buffers: independent of the host's block-size hint.
    constexpr int chunkSize = 64;
    std::array<std::array<float, chunkSize>, 2> wet{}, shifted{}, dry{};
    const float* wetPointers[] = {wet[0].data(), wet[1].data()};
    float* shiftedPointers[] = {shifted[0].data(), shifted[1].data()};
    auto* const* channels = buffer.getArrayOfWritePointers();
    const int numChannels = getTotalNumInputChannels();
    for (int offset = 0; offset < buffer.getNumSamples(); offset += chunkSize)
    {
        const int count = juce::jmin(chunkSize, buffer.getNumSamples() - offset);
        for (int i = 0; i < count; ++i)
        {
            const float gain = inputSmooth.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto c = static_cast<size_t>(ch);
                // A bad upstream sample must not poison the spectral and delay
                // histories. Finite audio (including values above unity) is unchanged.
                const float incoming = channels[ch][offset + i];
                const float sample = std::isfinite(incoming) ? incoming : 0.0f;
                inputBlockPeak = juce::jmax(inputBlockPeak, std::abs(sample));
                dry[c][static_cast<size_t>(i)] = dryDelay.getSample(ch, dryPosition);
                dryDelay.setSample(ch, dryPosition, sample);
                wet[c][static_cast<size_t>(i)] = sample * gain;
            }
            if (++dryPosition == getLatencySamples()) dryPosition = 0;
        }
        pitchShifter.process(wetPointers, shiftedPointers, count);
        detectedPitchHz.store(pitchShifter.detectedHz(), std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
        {
            const float mix = mixSmooth.getNextValue();
            const float output = outputSmooth.getNextValue();
            const float active = activeSmooth.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto c = static_cast<size_t>(ch), index = static_cast<size_t>(i);
                const float clean = dry[c][index];
                const float processed = (clean + mix * (shifted[c][index] - clean)) * output;
                channels[ch][offset + i] = active == 0.0f ? clean : clean + active * (processed - clean);
                outputBlockPeak = juce::jmax(outputBlockPeak, std::abs(channels[ch][offset + i]));
            }
        }
    }
    // Single producer and UI consumer; a racing UI reset may miss a peak, which
    // is acceptable for display. No CAS retry loop or interaction with audio.
    inputPeak.store(juce::jmax(inputPeak.load(std::memory_order_relaxed), inputBlockPeak), std::memory_order_relaxed);
    outputPeak.store(juce::jmax(outputPeak.load(std::memory_order_relaxed), outputBlockPeak), std::memory_order_relaxed);
}

void VocalisAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    render(buffer, false);
}

void VocalisAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    render(buffer, true);
}

juce::AudioProcessorParameter* VocalisAudioProcessor::getBypassParameter() const
{
    return parameters.getParameter(ParameterIDs::bypass);
}

void VocalisAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void VocalisAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            // Phase 1 sessions contain no pitch property: restore them at unity,
            // even if this instance previously had a nonzero pitch setting.
            // Old sessions had uncompensated pitch. Keep their timbre; new
            // instances default to 100% preserve via the parameter layout.
            const auto addMissing = [&state](const char* id, float value)
            {
                for (const auto& child : state)
                    if (child.getProperty("id").toString() == id) return;
                juce::ValueTree child("PARAM");
                child.setProperty("id", id, nullptr);
                child.setProperty("value", value, nullptr);
                state.appendChild(child, nullptr);
            };
            addMissing(ParameterIDs::pitch, 0.0f);
            addMissing(ParameterIDs::formantShift, 0.0f);
            addMissing(ParameterIDs::formantPreserve, 0.0f);
            addMissing(ParameterIDs::key, 0.0f);
            addMissing(ParameterIDs::scale, 2.0f);
            addMissing(ParameterIDs::correctionAmount, 0.0f);
            addMissing(ParameterIDs::retuneSpeed, 50.0f);
            addMissing(ParameterIDs::noteTransition, 50.0f);
            addMissing(ParameterIDs::hardTune, 0.0f);
            addMissing(ParameterIDs::humanize, 0.0f);
            addMissing(ParameterIDs::vibratoPreserve, 0.0f);
            addMissing(ParameterIDs::pitchDrift, 0.0f);
            addMissing(ParameterIDs::transitionSmoothness, 0.0f);
            addMissing(ParameterIDs::bloom, 0.0f);
            // Validate imported values off the audio path before APVTS publishes
            // them. A malformed state must not introduce NaN into a smoother.
            for (auto child : state)
                if (auto* parameter = parameters.getParameter(child.getProperty("id").toString()))
                {
                    float value = static_cast<float>(child.getProperty("value"));
                    if (!std::isfinite(value)) value = parameter->convertFrom0to1(parameter->getDefaultValue());
                    child.setProperty("value", parameter->getNormalisableRange().snapToLegalValue(value), nullptr);
                }
            parameters.replaceState(state);
        }
}

juce::AudioProcessorEditor* VocalisAudioProcessor::createEditor()
{
    return new VocalisAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalisAudioProcessor();
}
