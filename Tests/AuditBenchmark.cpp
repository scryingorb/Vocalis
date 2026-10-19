#include "../Source/PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void set(VocalisAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

// Opt-in offline timing study, not a pass/fail real-time certification. Synthesis,
// parameter notifications, buffer copies and output checks are outside timing.
void runAuditBenchmark()
{
    std::ofstream csv("audit-cpu.csv");
    if (!csv) throw std::runtime_error("Cannot open audit-cpu.csv");
    csv << "scenario,rate,channels,buffer,latency_samples,mean_us,p99_us,max_us,"
           "cpu_percent,deadline_misses,callbacks\n";
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        // Voiced notes, vibrato, a short unvoiced gap, and a repeatable breath bed.
        const int duration = static_cast<int>(rate * 2.0);
        std::vector<float> source(static_cast<size_t>(duration));
        juce::Random random(281);
        double phase = 0;
        for (int i = 0; i < duration; ++i)
        {
            const double time = i / rate;
            const double hz = (time < 1 ? 225.0 : 283.0)
                * std::pow(2.0, 0.15 * std::sin(6.283185307179586 * 5 * time) / 12);
            phase += 6.283185307179586 * hz / rate;
            double value = 0;
            if (time < 0.9 || time > 1.0)
                for (int harmonic = 1; harmonic <= 12; ++harmonic)
                    value += 0.06 * std::sin(phase * harmonic) / harmonic;
            source[static_cast<size_t>(i)] = static_cast<float>(value)
                + (random.nextFloat() - 0.5f) * 0.001f;
        }
        for (int channels : {1, 2})
            for (int size : {64, 128, 256, 512})
                for (int scenario = 0; scenario < 4; ++scenario)
                {
                    VocalisAudioProcessor processor;
                    processor.setPlayConfigDetails(channels, channels, rate, size);
                    if (scenario == 1) processor.presets.apply(0); // Natural
                    if (scenario >= 2) processor.presets.apply(2); // Hard Tune
                    if (scenario == 3)
                    {
                        set(processor, ParameterIDs::pitch, -5);
                        set(processor, ParameterIDs::formantShift, 2);
                    }
                    processor.prepareToPlay(rate, size);
                    juce::AudioBuffer<float> buffer(channels, size);
                    juce::MidiBuffer midi;
                    auto fill = [&](int offset, int count)
                    {
                        buffer.setSize(channels, count, false, false, true);
                        for (int ch = 0; ch < channels; ++ch)
                            std::copy_n(source.data() + offset, count, buffer.getWritePointer(ch));
                    };
                    // Prime detection and spectral history without timing initialization.
                    for (int offset = 0; offset + size <= static_cast<int>(rate * 0.5); offset += size)
                    {
                        fill(offset, size);
                        processor.processBlock(buffer, midi);
                    }
                    std::vector<double> times;
                    times.reserve(static_cast<size_t>(duration / size + 1));
                    double total = 0;
                    int misses = 0, nextAutomation = 0;
                    bool toggle = false;
                    for (int offset = 0; offset < duration; offset += size)
                    {
                        const int count = std::min(size, duration - offset);
                        fill(offset, count);
                        if (scenario == 3 && offset >= nextAutomation)
                        {
                            toggle = !toggle;
                            set(processor, ParameterIDs::pitch, toggle ? 7.0f : -5.0f);
                            set(processor, ParameterIDs::formantShift, toggle ? -3.0f : 2.0f);
                            set(processor, ParameterIDs::dryWet, toggle ? 65.0f : 100.0f);
                            set(processor, ParameterIDs::retuneSpeed, toggle ? 40.0f : 100.0f);
                            nextAutomation += static_cast<int>(rate * 0.05);
                        }
                        const auto start = std::chrono::steady_clock::now();
                        processor.processBlock(buffer, midi);
                        const double us = std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - start).count();
                        times.push_back(us);
                        total += us;
                        misses += us > count * 1e6 / rate;
                        for (int ch = 0; ch < channels; ++ch)
                            for (int i = 0; i < count; ++i)
                                if (!std::isfinite(buffer.getSample(ch, i)))
                                    throw std::runtime_error("Non-finite audit benchmark output");
                    }
                    std::sort(times.begin(), times.end());
                    const char* names[] = {"unity", "natural", "hard", "automation"};
                    csv << names[scenario] << ',' << rate << ',' << channels << ',' << size << ','
                        << processor.getLatencySamples() << ',' << total / times.size() << ','
                        << times[static_cast<size_t>(times.size() * 0.99)] << ',' << times.back() << ','
                        << total / (duration / rate * 1e4) << ',' << misses << ',' << times.size() << '\n';
                }
    }
    if (!csv) throw std::runtime_error("Cannot write audit-cpu.csv");
    std::cout << "Measured 96 production processor configurations in audit-cpu.csv; "
                 "offline callback timing only, no ASIO/DAW certification.\n";
}
