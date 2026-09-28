#include "../Source/PluginProcessor.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <complex>

void processWithoutAllocations(juce::AudioProcessor&, juce::AudioBuffer<float>&, juce::MidiBuffer&, bool);

namespace
{
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(VocalisAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

void runLatencyTests()
{
    // A coherent stereo tone with a deliberate phase/level difference must not
    // drift apart when the LIVE engines use independent spectral maps.
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
        for (float pitch : {0.0f, 3.0f, -5.5f, 12.0f, -12.0f})
        {
            const int count = static_cast<int>(rate * 0.7);
            std::vector<float> left(static_cast<size_t>(count)), right(left.size());
            std::vector<float> outLeft(left.size()), outRight(left.size());
            constexpr double phase = 0.6;
            for (int i = 0; i < count; ++i)
            {
                const double angle = juce::MathConstants<double>::twoPi * 220.0 * i / rate;
                left[static_cast<size_t>(i)] = 0.1f * static_cast<float>(std::sin(angle));
                right[static_cast<size_t>(i)] = 0.05f * static_cast<float>(std::sin(angle + phase));
            }
            PitchShifter shifter;
            shifter.prepare(rate, 2, pitch, PitchShifter::Mode::live, 0, 0);
            const float* in[] = {left.data(), right.data()};
            float* out[] = {outLeft.data(), outRight.data()};
            shifter.process(in, out, count);
            const double frequency = 220.0 * std::pow(2.0, pitch / 12.0);
            double firstPhase = 0;
            for (int section = 0; section < 2; ++section)
            {
                std::complex<double> a{}, b{};
                const int start = static_cast<int>(rate * (0.3 + section * 0.2));
                const int end = static_cast<int>(rate * (0.5 + section * 0.2));
                for (int i = start; i < end; ++i)
                {
                    const auto rotation = std::polar(1.0, -juce::MathConstants<double>::twoPi * frequency * i / rate);
                    a += static_cast<double>(outLeft[static_cast<size_t>(i)]) * rotation;
                    b += static_cast<double>(outRight[static_cast<size_t>(i)]) * rotation;
                }
                const double measuredPhase = std::arg(b * std::conj(a));
                if (section == 0) firstPhase = measuredPhase;
                // Independent pitch maps may change the stereo phase offset;
                // unlike linked QUALITY, they promise clock alignment, not an
                // unchanged image. Check drift, not the original phase offset.
                if (std::abs(measuredPhase - firstPhase) >= 0.03)
                    std::cerr << "Stereo phase drift: rate=" << rate << " pitch=" << pitch
                              << " first=" << firstPhase << " later=" << measuredPhase << '\n';
                require(std::abs(measuredPhase - firstPhase) < 0.03, "Stereo relative phase drift");
                if (pitch == 0) require(std::abs(measuredPhase - phase) < 0.03, "Zero-pitch stereo phase changed");
                require(std::abs(std::abs(b) / std::abs(a) - 0.5) < 0.02, "Stereo level relationship changed");
            }
        }
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
        for (int channels : {1, 2})
            for (float pitch : {0.0f, 3.0f, -5.5f, 12.0f, -12.0f})
            for (float formant : {0.0f, 3.0f, -3.0f})
            {
                // Same source/seed in independently prepared wet, dry, mix,
                // parameter-bypass and host-bypass instances.
                std::array<VocalisAudioProcessor, 5> processors;
                for (auto& p : processors)
                {
                    p.setPlayConfigDetails(channels, channels, rate, 64);
                    set(p, "pitch", pitch);
                    set(p, "formantShift", formant);
                }
                set(processors[1], "dryWet", 0);
                set(processors[2], "dryWet", 50);
                set(processors[3], "bypass", 1);
                for (auto& p : processors) p.prepareToPlay(rate, 64);
                const int latency = processors[0].getLatencySamples();
                const auto config = PitchShifter::configurationFor(PitchShifter::Mode::live);
                require(latency == static_cast<int>(rate * config.blockSeconds) + 64,
                    "Unexpected LIVE latency");
                for (auto& p : processors) require(p.getLatencySamples() == latency, "Mode-dependent path latency");

                const int length = static_cast<int>(rate * 0.6);
                std::vector<float> source(static_cast<size_t>(length));
                juce::Random random(123);
                for (int i = 0; i < length; ++i)
                {
                    // Broadband signal, plus sparse markers for timing drift.
                    source[static_cast<size_t>(i)] = (random.nextFloat() - 0.5f) * 0.1f;
                    if (i % 4001 == 0) source[static_cast<size_t>(i)] += 0.2f;
                }
                std::array<juce::AudioBuffer<float>, 5> buffers;
                for (auto& b : buffers) b.setSize(channels, 4096);
                juce::MidiBuffer midi;
                const int sizes[] = {1, 17, 64, 128, 511, 1024, 4096};
                int block = 0;
                for (int offset = 0; offset < length;)
                {
                    const int count = juce::jmin(sizes[block++ % 7], length - offset);
                    for (size_t p = 0; p < processors.size(); ++p)
                    {
                        auto& buffer = buffers[p];
                        buffer.setSize(channels, count, false, false, true);
                        for (int ch = 0; ch < channels; ++ch)
                            std::copy_n(source.data() + offset, count, buffer.getWritePointer(ch));
                        processWithoutAllocations(processors[p], buffer, midi, p == 4);
                    }
                    for (int i = 0; i < count; ++i)
                    {
                        const int original = offset + i - latency;
                        const float clean = original >= 0 ? source[static_cast<size_t>(original)] : 0.0f;
                        for (int ch = 0; ch < channels; ++ch)
                        {
                            const float wet = buffers[0].getSample(ch, i);
                            require(std::abs(buffers[2].getSample(ch, i) - (clean + 0.5f * (wet - clean))) < 1e-7f,
                                "Dry/wet paths are not time-aligned");
                            require(buffers[1].getSample(ch, i) == clean, "Dry path timing drift");
                            require(buffers[3].getSample(ch, i) == clean, "Parameter bypass timing drift");
                            if (offset + i > rate * 0.021)
                                require(buffers[4].getSample(ch, i) == clean, "Host bypass timing drift");
                            if (pitch == 0 && formant == 0) require(wet == clean, "Zero pitch dry/wet comb filtering");
                            if (ch == 1) require(std::abs(wet - buffers[0].getSample(0, i)) < 1e-6f,
                                "Stereo channels desynchronised");
                        }
                    }
                    offset += count;
                }
            }
    // The retained QUALITY option is preparation-only and must still be viable.
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        PitchShifter shifter;
        shifter.prepare(rate, 1, 0, PitchShifter::Mode::quality);
        require(shifter.latencySamples() == static_cast<int>(rate * 0.12) + 64, "QUALITY latency changed");
        std::vector<float> input(static_cast<size_t>(shifter.latencySamples() + 1)), output(input.size());
        input[0] = 0.25f;
        const float* in[] = {input.data()}; float* out[] = {output.data()};
        shifter.process(in, out, static_cast<int>(input.size()));
        require(output.back() == 0.25f, "QUALITY impulse timing failed");
    }
    std::cout << "PASS: LIVE/QUALITY timing, dry/wet and both bypass paths, stereo sync\n";
}
