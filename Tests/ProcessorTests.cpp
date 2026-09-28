#include "../Source/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

void runPitchTests();
void runCorrectionTests();
void runHumanizerTests();
void runPresetTests();
void runUiTests();
void runStabilityTests();
void runUltraLiveTests();
void runActiveLiveProbes();
void runLiveAuditions();
void runLiveActiveCpu();
void runVocalValidation();
void runVocalFocusedTests();
void runLiveStudy(const juce::String&);
void runPresetEditorTest(juce::AudioProcessorEditor&, VocalisAudioProcessor&);
void runFormantTests();
void runFormantBenchmark();
void runLatencyBenchmark(const juce::String& filter);
void runLatencyTests();
void runHostedPitchTest(juce::AudioProcessor&);

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void set(VocalisAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void checkGain(juce::AudioProcessor& p, int channels, float expected, bool bypassed = false)
{
    juce::AudioBuffer<float> buffer(channels, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 30 + p.getLatencySamples() / 256; ++block)
    {
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(ch, i, ch == 0 ? 0.1f : -0.2f);
        if (bypassed) p.processBlockBypassed(buffer, midi);
        else p.processBlock(buffer, midi);
    }
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            require(std::abs(buffer.getSample(ch, i) - (ch == 0 ? 0.1f : -0.2f) * expected) < 0.000002f,
                    "Unexpected sample gain or channel crosstalk");
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try
    {
        if (argc > 1 && juce::String(argv[1]) == "--vocal-focused") { runVocalFocusedTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--vocal-qa") { runVocalValidation(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--active-cpu") { runLiveActiveCpu(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--live-auditions") { runActiveLiveProbes(); runLiveAuditions(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--ultra-tests") { runUltraLiveTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--live-study") { runLiveStudy(argc > 2 ? argv[2] : "phase11-study"); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--stability-tests") { runStabilityTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--ui-tests") { runUiTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--preset-tests") { runPresetTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--humanizer-tests") { runHumanizerTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--correction-tests") { runCorrectionTests(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--formant-benchmark") { runFormantBenchmark(); return 0; }
        if (argc > 1 && juce::String(argv[1]) == "--benchmark")
        {
            runLatencyBenchmark(argc > 2 ? juce::String(argv[2]) : juce::String{});
            return 0;
        }
        for (const int channels : {1, 2})
        {
            VocalisAudioProcessor p;
            auto layout = p.getBusesLayout();
            layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
            layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
            require(p.setBusesLayout(layout), "Mono/stereo layout rejected");
            auto invalid = layout;
            invalid.outputBuses.set(0, juce::AudioChannelSet::create5point1());
            require(!p.isBusesLayoutSupported(invalid), "Surround layout accepted");
            for (const double rate : {44100.0, 48000.0, 96000.0})
            {
                set(p, "inputGain", 0); set(p, "outputGain", 0); set(p, "dryWet", 100); set(p, "bypass", 0);
                p.prepareToPlay(rate, 256);
                checkGain(p, channels, 1.0f);
                set(p, "inputGain", 6);
                const float gain = juce::Decibels::decibelsToGain(6.0f);
                // Automation must ramp rather than jump, including across blocks.
                const int rampLength = p.getLatencySamples() + 256;
                juce::AudioBuffer<float> ramp(channels, rampLength);
                juce::MidiBuffer rampMidi;
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < rampLength; ++i)
                        ramp.setSample(ch, i, 0.1f);
                p.processBlock(ramp, rampMidi);
                const int start = p.getLatencySamples();
                require(ramp.getSample(0, start) > 0.1f && ramp.getSample(0, start) < 0.101f,
                        "Gain automation did not start smoothly");
                for (int i = start + 1; i < rampLength; ++i)
                    require(ramp.getSample(0, i) >= ramp.getSample(0, i - 1)
                        && ramp.getSample(0, i) - ramp.getSample(0, i - 1) < 0.001f,
                        "Gain automation ramp discontinuity");
                for (int ch = 1; ch < channels; ++ch)
                    for (int i = start; i < rampLength; ++i)
                        require(ramp.getSample(ch, i) == ramp.getSample(0, i), "Stereo ramp mismatch");
                checkGain(p, channels, gain);
                set(p, "dryWet", 0); checkGain(p, channels, 1.0f);
                set(p, "dryWet", 50); checkGain(p, channels, (1.0f + gain) * 0.5f);
                set(p, "outputGain", -6); checkGain(p, channels, (1.0f + gain) * 0.5f / gain);
                set(p, "bypass", 1); checkGain(p, channels, 1.0f);
                set(p, "bypass", 0); checkGain(p, channels, 1.0f, true);
                checkGain(p, channels, (1.0f + gain) * 0.5f / gain);
                juce::AudioBuffer<float> empty(channels, 0);
                juce::MidiBuffer midi;
                p.processBlock(empty, midi);
            }
            set(p, "bypass", 1);
            set(p, "pitch", 2.25f);
            set(p, "formantShift", -1.5f); set(p, "formantPreserve", 37);
            juce::MemoryBlock state;
            p.getStateInformation(state);
            VocalisAudioProcessor restored;
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            for (const auto* id : {"inputGain", "dryWet", "outputGain", "bypass", "pitch", "formantShift", "formantPreserve"})
                require(p.parameters.getRawParameterValue(id)->load() == restored.parameters.getRawParameterValue(id)->load(),
                        "State round-trip failed");
            restored.setStateInformation("bad", 3);
            require(restored.parameters.getRawParameterValue("inputGain")->load() == 6.0f, "Invalid state damaged parameters");
            auto legacy = p.parameters.copyState();
            for (int i = legacy.getNumChildren() - 1; i >= 0; --i)
                if (legacy.getChild(i).getProperty("id").toString() == "pitch") legacy.removeChild(i, nullptr);
            juce::MemoryBlock legacyData;
            juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyData);
            restored.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
            require(restored.parameters.getRawParameterValue("pitch")->load() == 0.0f, "Phase 1 state did not reset pitch");
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
            require(editor->getWidth() == 1120 && editor->getHeight() == 900, "Editor creation failed");
            runPresetEditorTest(*editor, p);
            auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
            require(snapshot.isValid(), "Editor render failed");
            if (channels == 2)
            {
                juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile("Vocalis-ui.png"));
                stream.setPosition(0);
                stream.truncate();
                juce::PNGImageFormat png;
                require(png.writeImageToStream(snapshot, stream), "UI snapshot failed");
            }
        }
        require(argc > 1, "Missing VST3 bundle path");
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions, juce::String(argv[1]));
        require(descriptions.size() == 1, "VST3 discovery failed");
        require(descriptions[0]->version == "1.0.0", "VST3 release version differs");
        juce::String error;
        auto plugin = format.createInstanceFromDescription(*descriptions[0], 48000, 256, error);
        require(plugin != nullptr, error.toRawUTF8());
        require(plugin->getParameters().size() == 18, "VST3 parameter count differs");
        plugin->prepareToPlay(48000, 256);
        checkGain(*plugin, 2, 1.0f);
        runHostedPitchTest(*plugin);
        plugin->releaseResources();
        runPitchTests();
        runLatencyTests();
        runFormantTests();
        runCorrectionTests();
        runHumanizerTests();
        runPresetTests();
        runUiTests();
        runStabilityTests();
        runUltraLiveTests();
        runVocalFocusedTests();
        std::cout << "PASS: Vocalis 1.0.0 RC stable processor plus isolated experimental engine, pitch DSP, state, UI and VST3 checks\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

