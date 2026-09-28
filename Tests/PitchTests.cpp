#include "../Source/PluginProcessor.h"
#include "../Source/ui/BubbleKnob.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace
{
thread_local bool trackAllocations = false;
thread_local size_t allocationCount = 0;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void setValue(VocalisAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

std::vector<float> render(double rate, int channels, float semitones, const std::vector<int>& blocks,
                          double fundamental = 220.0)
{
    VocalisAudioProcessor p;
    setValue(p, "formantPreserve", 0);
    p.setPlayConfigDetails(channels, channels, rate, 64);
    setValue(p, "pitch", semitones);
    p.prepareToPlay(rate, 64); // Deliberately smaller than many actual blocks.
    const int length = static_cast<int>(rate);
    std::vector<float> result(static_cast<size_t>(length * channels));
    juce::AudioBuffer<float> buffer(channels, 4096);
    juce::MidiBuffer midi;
    int offset = 0, block = 0;
    while (offset < length)
    {
        const int size = juce::jmin(blocks[static_cast<size_t>(block++) % blocks.size()], length - offset);
        buffer.setSize(channels, size, false, false, true);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < size; ++i)
                buffer.setSample(ch, i, 0.1f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi
                    * (ch == 0 ? fundamental : fundamental * 1.5) * (offset + i) / rate)));
        allocationCount = 0;
        trackAllocations = true;
        p.processBlock(buffer, midi);
        trackAllocations = false;
        check(allocationCount == 0, "Audio callback allocated via C++ new");
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < size; ++i)
            {
                const float sample = buffer.getSample(ch, i);
                check(std::isfinite(sample) && std::abs(sample) < 0.5f, "Non-finite or runaway shifted output");
                result[static_cast<size_t>(ch * length + offset + i)] = sample;
            }
        offset += size;
    }
    return result;
}

double frequency(const float* samples, int start, int end, double rate)
{
    double first = 0, last = 0;
    int crossings = 0;
    for (int i = start + 1; i < end; ++i)
        if (samples[i - 1] <= 0 && samples[i] > 0)
        {
            last = i - 1 + static_cast<double>(-samples[i - 1]) / (samples[i] - samples[i - 1]);
            if (crossings++ == 0) first = last;
        }
    check(crossings > 10, "Shifted signal missing or too quiet");
    return (crossings - 1) * rate / (last - first);
}
}

// Test executable only: intercept C++ allocations, including aligned/array forms.
void* operator new(std::size_t n)
{
    if (trackAllocations) ++allocationCount;
    if (auto* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment)
{
    if (trackAllocations) ++allocationCount;
    if (auto* p = _aligned_malloc(n == 0 ? 1 : n, static_cast<size_t>(alignment))) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }

void processWithoutAllocations(juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& midi, bool bypassed)
{
    allocationCount = 0;
    trackAllocations = true;
    if (bypassed) processor.processBlockBypassed(buffer, midi);
    else processor.processBlock(buffer, midi);
    trackAllocations = false;
    check(allocationCount == 0, "Broadband audio callback allocated");
}

void processDspWithoutAllocations(PitchShifter& dsp, const float* const* input, float* const* output, int count)
{
    allocationCount = 0;
    trackAllocations = true;
    dsp.process(input, output, count);
    trackAllocations = false;
    check(allocationCount == 0, "Transform callback allocated");
}

void runPitchTests()
{
    BubbleKnob knob;
    check(knob.getTextFromValue(0) == "0.00 st" && knob.getTextFromValue(2.25) == "+2.25 st"
        && knob.getTextFromValue(-5.5) == "-5.50 st", "Pitch knob formatting failed");
    check(knob.getValueFromText("+2.25 st") == 2.25, "Fractional pitch text entry failed");

    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        VocalisAudioProcessor p;
    setValue(p, "formantPreserve", 0);
        p.prepareToPlay(rate, 64);
        const int latency = p.getLatencySamples();
        std::cout << "Latency at " << rate << " Hz: " << latency << " samples\n";
        check(latency > 0 && p.getTailLengthSeconds() >= latency / rate, "Latency/tail reporting failed");
        juce::AudioBuffer<float> buffer(2, latency + 100);
        buffer.clear();
        buffer.setSample(0, 0, 0.125f);
        buffer.setSample(1, 13, -0.25f);
        juce::MidiBuffer midi;
        p.processBlock(buffer, midi);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            check(buffer.getSample(0, i) == (i == latency ? 0.125f : 0.0f), "Zero pitch is not sample-exact at reported latency");
            check(buffer.getSample(1, i) == (i == latency + 13 ? -0.25f : 0.0f), "Zero pitch stereo alignment failed");
        }
        for (int channels : {1, 2})
            for (float pitch : {0.0f, 3.0f, -5.5f, -12.0f, -3.5f, 2.25f, 7.0f, 12.0f})
            {
                const auto output = render(rate, channels, pitch, {1, 32, 64, 127, 256, 511, 1024, 4096});
                const int length = static_cast<int>(rate);
                for (int ch = 0; ch < channels; ++ch)
                {
                    const double expected = (ch == 0 ? 220.0 : 330.0) * std::pow(2.0, pitch / 12.0);
                    const double measured = frequency(output.data() + ch * length, length / 2, length, rate);
                    // Spectral peak mapping is approximate, particularly for
                    // unrelated tones in a stereo pair. Bound error in cents.
                    const double cents = std::abs(1200.0 * std::log2(measured / expected));
                    const double tolerance = 10.0;
                    if (cents >= tolerance)
                    {
                        std::cerr << "rate=" << rate << " channels=" << channels << " pitch=" << pitch
                                  << " channel=" << ch << " expected=" << expected << " measured=" << measured << '\n';
                    }
                    check(cents < tolerance, "Measured pitch differs from requested semitones");
                }
            }
    }
    const auto fixed = render(48000, 2, 7, {256});
    const auto variable = render(48000, 2, 7, {1, 31, 127, 1024, 4096});
    check(fixed == variable, "Pitch result depends on host block partitioning");

    // Include low vocal notes, where short windows have the largest error.
    // Keep this LIVE compromise explicit; QUALITY's measured error is lower.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (double inputHz : {80.0, 110.0})
            for (float pitch : {3.0f, -5.5f, 12.0f, -12.0f})
            {
                const auto output = render(rate, 1, pitch, {31, 64, 127, 256, 1024}, inputHz);
                const double expected = inputHz * std::pow(2.0, pitch / 12.0);
                const double measured = frequency(output.data(), static_cast<int>(rate * 0.4), static_cast<int>(rate), rate);
                check(std::abs(1200 * std::log2(measured / expected)) < 35, "LIVE low-note pitch error exceeds measured budget");
                const double early = frequency(output.data(), static_cast<int>(rate * 0.3), static_cast<int>(rate * 0.6), rate);
                const double late = frequency(output.data(), static_cast<int>(rate * 0.65), static_cast<int>(rate * 0.95), rate);
                check(std::abs(1200 * std::log2(late / early)) < 1, "Steady low note has unstable pitch");
            }

    VocalisAudioProcessor p;
    setValue(p, "formantPreserve", 0);
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    float previous = 0;
    float maxStep = 0;
    for (int block = 0; block < 600; ++block)
    {
        if (block == 30) setValue(p, "pitch", 12);
        if (block == 100) setValue(p, "pitch", -12);
        if (block == 200) setValue(p, "bypass", 1);
        if (block == 250) setValue(p, "bypass", 0);
        if (block == 300) setValue(p, "pitch", 0);
        for (int i = 0; i < 256; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample(ch, i, 0.1f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi
                    * 220.0 * (block * 256 + i) / 48000.0)));
        allocationCount = 0; trackAllocations = true;
        p.processBlock(buffer, midi);
        trackAllocations = false;
        check(allocationCount == 0, "Pitch/bypass automation allocated");
        for (int i = 0; i < 256; ++i)
        {
            const float sample = buffer.getSample(0, i);
            check(std::isfinite(sample), "Automation produced non-finite output");
            maxStep = juce::jmax(maxStep, std::abs(sample - previous));
            previous = sample;
            if ((block > 220 && block < 250) || block > 500)
            {
                const float original = 0.1f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi
                    * 220.0 * (block * 256 + i - p.getLatencySamples()) / 48000.0));
                check(sample == original, "Bypass/return-to-zero did not recover exact delayed input");
            }
        }
    }
    check(maxStep < 0.08f, "Large discontinuity during pitch/bypass automation");
    allocationCount = 0; trackAllocations = true;
    p.reset();
    trackAllocations = false;
    check(allocationCount == 0, "Reset allocated after preparation");
    for (int block = 0; block < 100; ++block)
    {
        buffer.clear();
        p.processBlock(buffer, midi);
        check(buffer.getMagnitude(0, 256) == 0.0f, "Reset leaked old audio into silence");
    }
    std::cout << "Pitch frequency, variable buffers, zero null, automation and allocation checks passed; max step " << maxStep << '\n';
}

void runHostedPitchTest(juce::AudioProcessor& plugin)
{
    plugin.getParameters()[6]->setValueNotifyingHost(0);
    auto* pitch = plugin.getParameters()[4];
    check(pitch->getName(32) == "Pitch", "VST3 Pitch parameter missing");
    pitch->setValueNotifyingHost(19.0f / 24.0f); // +7 st, normalized [-12, 12]
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    std::vector<float> output(48000);
    for (int offset = 0; offset < 48000; offset += 256)
    {
        const int count = juce::jmin(256, 48000 - offset);
        buffer.setSize(2, count, false, false, true);
        for (int i = 0; i < count; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample(ch, i, 0.1f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi
                    * 220.0 * (offset + i) / 48000.0)));
        plugin.processBlock(buffer, midi);
        std::copy_n(buffer.getReadPointer(0), count, output.data() + offset);
    }
    const double measured = frequency(output.data(), 24000, 48000, 48000);
    check(std::abs(measured / (220 * std::pow(2.0, 7.0 / 12.0)) - 1) < 0.003,
        "Actual VST3 did not shift pitch");
    check(plugin.getLatencySamples() == 2368, "VST3 did not expose LIVE latency");
}

#include "../Source/dsp/live/UltraLiveEngine.h"
void processUltraWithoutAllocations(UltraLiveEngine& dsp,const float* const* in,float* const* out,int n)
{
    allocationCount=0;trackAllocations=true;dsp.process(in,out,n);trackAllocations=false;
    check(allocationCount==0,"ULTRA callback allocated");
}