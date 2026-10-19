#include "../Source/dsp/PitchShifter.h"
#include <juce_audio_utils/juce_audio_utils.h>
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <signalsmith-stretch/signalsmith-stretch.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include <chrono>
#include <fstream>
#include <iomanip>
#include <vector>
#include <stdexcept>

namespace
{
struct Candidate { const char* name; PitchShifter::Configuration config; };
const Candidate candidates[] = {
    {"quality120", {0.12, 0.015, false}},
    {"60ms", {0.06, 0.0075, false}},
    {"40ms", {0.04, 0.005, false}},
    {"30ms", {0.03, 0.00375, false}},
    {"20ms", {0.02, 0.0025, false}},
    {"40ms_4x", {0.04, 0.01, false}},
    {"40ms_split", {0.04, 0.005, true}},
    {"40ms_16x", {0.04, 0.0025, false}},
    {"48ms", {0.048, 0.006, false}},
    {"48ms_16x", {0.048, 0.003, false}},
    {"60ms_16x", {0.06, 0.00375, false}},
    {"80ms", {0.08, 0.01, false}},
    {"30ms_32x", {0.03, 0.0009375, false}},
    {"40ms_32x", {0.04, 0.00125, false}},
    {"48ms_32x", {0.048, 0.0015, false}},
    {"40ms_64x", {0.04, 0.000625, false}},
    {"40ms_32x_split", {0.04, 0.00125, true}},
    {"live40", {0.04, 0.00125, false, false}},
    {"live40_split", {0.04, 0.00125, true, false}},
    {"live40_24x", {0.04, 0.04 / 24.0, false, false}},
    {"live48_16x", {0.048, 0.003, false, false}},
    {"live48_split", {0.048, 0.003, true, false}}
};
using Clock = std::chrono::steady_clock;
double freq(const std::vector<float>& signal, int start, int end, double rate)
{
    int count = 0;
    double first = 0, last = 0;
    for (int i = start + 1; i < end; ++i)
        if (signal[static_cast<size_t>(i - 1)] <= 0 && signal[static_cast<size_t>(i)] > 0)
        {
            last = i - 1 + static_cast<double>(-signal[static_cast<size_t>(i - 1)])
                / (signal[static_cast<size_t>(i)] - signal[static_cast<size_t>(i - 1)]);
            if (count++ == 0) first = last;
        }
    return count > 2 ? (count - 1) * rate / (last - first) : 0;
}
std::vector<float> shift(const std::vector<float>& input, double rate, float pitch,
                         PitchShifter::Configuration config)
{
    PitchShifter shifter;
    shifter.prepare(rate, 1, pitch, config, 0, 0);
    std::vector<float> output(input.size());
    const float* in[] = {input.data()};
    float* out[] = {output.data()};
    shifter.process(in, out, static_cast<int>(input.size()));
    return output;
}
double energyWidthMs(const std::vector<float>& signal, double rate)
{
    double total = 0;
    for (float sample : signal) total += static_cast<double>(sample) * sample;
    double sum = 0;
    int first = -1, last = 0;
    for (int i = 0; i < static_cast<int>(signal.size()); ++i)
    {
        sum += static_cast<double>(signal[static_cast<size_t>(i)]) * signal[static_cast<size_t>(i)];
        if (first < 0 && sum >= total * 0.05) first = i;
        if (sum <= total * 0.95) last = i;
    }
    return (last - first + 1) * 1000.0 / rate;
}
void writeWave(const juce::File& file, const std::vector<float>& audio, double rate)
{
    auto fileStream = file.createOutputStream();
    fileStream->setPosition(0); fileStream->truncate();
    std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
        .withSampleRate(rate).withNumChannels(1).withBitsPerSample(24));
    if (!writer) throw std::runtime_error("Cannot write audition WAV");
    const float* channels[] = {audio.data()};
    writer->writeFromFloatArrays(channels, 1, static_cast<int>(audio.size()));
}
}

void runLatencyBenchmark(const juce::String& filter)
{
    auto folder = juce::File::getCurrentWorkingDirectory().getChildFile("latency-benchmark");
    if (filter.isNotEmpty()) folder = folder.getChildFile(filter);
    folder.createDirectory();
    std::ofstream csv(folder.getChildFile("configurations.csv").getFullPathName().toStdString());
    std::ofstream tones(folder.getChildFile("tones.csv").getFullPathName().toStdString());
    std::ofstream stereo(folder.getChildFile("stereo-tones.csv").getFullPathName().toStdString());
    stereo << "config,rate,semitones,channel,error_cents\n";
    csv << "config,rate,block,hop,split,reported_samples,impulse_samples,spectral_unity_samples,host_buffer,cpu_percent,p99_us,max_us,deadline_overruns,shifted_impulse_width90_ms,shifted_impulse_centroid_ms,input_burst_width90_ms,shifted_burst_width90_ms\n";
    tones << "config,rate,input_hz,semitones,error_cents,instability_cents\n";
    for (const auto& candidate : candidates)
    {
        if (filter.isNotEmpty() && !juce::String(candidate.name).startsWith(filter)) continue;
        for (double rate : {44100.0, 48000.0, 96000.0})
        {
            const auto config = candidate.config;
            const int block = static_cast<int>(rate * config.blockSeconds);
            const int hop = static_cast<int>(rate * config.intervalSeconds);
            PitchShifter shifter;
            shifter.prepare(rate, 2, 0, config, 0, 0);
            const int latency = shifter.latencySamples();
            std::vector<float> impulse(static_cast<size_t>(rate), 0.0f);
            impulse[0] = 1;
            const auto unity = shift(impulse, rate, 0, config);
            const int peak = static_cast<int>(std::max_element(unity.begin(), unity.end()) - unity.begin());
            if (peak != latency) throw std::runtime_error("Reported unity latency mismatch");

            // Independently exercise the spectral path at unity, not the clean shortcut.
            signalsmith::stretch::SignalsmithStretch<float> spectral(0);
            spectral.configure(1, block, hop, config.splitComputation);
            spectral.reset(); spectral.setTransposeSemitones(0);
            std::vector<float> spectralOutput(impulse.size());
            const float* rawIn[] = {impulse.data()};
            float* rawOut[] = {spectralOutput.data()};
            spectral.process(rawIn, static_cast<int>(impulse.size()), rawOut, static_cast<int>(impulse.size()));
            const int spectralPeak = static_cast<int>(std::max_element(spectralOutput.begin(), spectralOutput.end()) - spectralOutput.begin());
            if (spectralPeak + 64 != latency) throw std::runtime_error("Spectral and dry latency mismatch");

            std::fill(impulse.begin(), impulse.end(), 0.0f);
            const int impulseAt = static_cast<int>(rate * 0.25);
            impulse[static_cast<size_t>(impulseAt)] = 1;
            const auto transient = shift(impulse, rate, 3, config);
            double energy = 0, centroid = 0;
            for (size_t i = 0; i < transient.size(); ++i)
            { const double e = transient[i] * transient[i]; energy += e; centroid += i * e; }
            double accumulated = 0;
            int low = -1, high = 0;
            for (int i = 0; i < static_cast<int>(transient.size()); ++i)
            {
                accumulated += transient[static_cast<size_t>(i)] * transient[static_cast<size_t>(i)];
                if (low < 0 && accumulated >= energy * 0.05) low = i;
                if (accumulated <= energy * 0.95) high = i;
            }
            const double width = (high - low + 1) * 1000.0 / rate;
            const double centroidMs = (centroid / energy - impulseAt) * 1000.0 / rate;
            std::vector<float> burst(static_cast<size_t>(rate));
            const int burstLength = static_cast<int>(rate * 0.015);
            for (int i = 0; i < burstLength; ++i)
            {
                const double envelope = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * i / burstLength);
                burst[static_cast<size_t>(impulseAt + i)] = static_cast<float>(envelope * 0.1
                    * std::sin(juce::MathConstants<double>::twoPi * 220 * i / rate));
            }
            const double inputBurstWidth = energyWidthMs(burst, rate);
            const double outputBurstWidth = energyWidthMs(shift(burst, rate, 3, config), rate);

            for (float pitch : {0.0f, 3.0f, -5.5f, 12.0f, -12.0f})
            {
                std::vector<float> left(static_cast<size_t>(rate)), right(left.size());
                std::vector<float> outLeft(left.size()), outRight(left.size());
                for (size_t i = 0; i < left.size(); ++i)
                {
                    left[i] = 0.1f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 220 * i / rate));
                    right[i] = 0.1f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 330 * i / rate));
                }
                shifter.prepare(rate, 2, pitch, config, 0, 0);
                const float* in[] = {left.data(), right.data()};
                float* out[] = {outLeft.data(), outRight.data()};
                shifter.process(in, out, static_cast<int>(left.size()));
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double measured = freq(channel == 0 ? outLeft : outRight,
                        static_cast<int>(rate * 0.4), static_cast<int>(rate), rate);
                    const double expected = (channel == 0 ? 220 : 330) * std::pow(2.0, pitch / 12.0);
                    stereo << candidate.name << ',' << rate << ',' << pitch << ',' << channel << ','
                           << 1200 * std::log2(measured / expected) << '\n';
                }
            }

            for (double hz : {80.0, 110.0, 220.0, 330.0})
                for (float pitch : {0.0f, 3.0f, -5.5f, 12.0f, -12.0f})
                {
                    std::vector<float> input(static_cast<size_t>(rate));
                    for (size_t i = 0; i < input.size(); ++i)
                        input[i] = 0.1f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * hz * i / rate));
                    const auto output = shift(input, rate, pitch, config);
                    const double expected = hz * std::pow(2.0, pitch / 12.0);
                    const double measured = freq(output, static_cast<int>(rate * 0.4), static_cast<int>(rate), rate);
                    double minimum = 1e9, maximum = -1e9;
                    for (int section = 0; section < 4; ++section)
                    {
                        const double f = freq(output, static_cast<int>(rate * (0.4 + section * 0.15)),
                            static_cast<int>(rate * (0.55 + section * 0.15)), rate);
                        const double cents = 1200 * std::log2(f / expected);
                        minimum = std::min(minimum, cents); maximum = std::max(maximum, cents);
                    }
                    tones << candidate.name << ',' << rate << ',' << hz << ',' << pitch << ','
                          << 1200 * std::log2(measured / expected) << ',' << maximum - minimum << '\n';
                }

            // Pre-render voiced harmonics + a little noise, so timing excludes synthesis.
            const int duration = static_cast<int>(rate * 3);
            std::vector<float> source(static_cast<size_t>(duration));
            juce::Random random(42);
            for (int i = 0; i < duration; ++i)
            {
                const double t = i / rate;
                double value = 0;
                for (int h = 1; h <= 16; ++h)
                    value += std::sin(juce::MathConstants<double>::twoPi * h * (130 * t + 0.08 * std::sin(31.4 * t))) / h;
                source[static_cast<size_t>(i)] = static_cast<float>(value * 0.08) + (random.nextFloat() - 0.5f) * 0.002f;
            }
            for (int hostBuffer : {64, 256})
            {
                shifter.prepare(rate, 2, -5.5f, config, 0, 0);
                std::vector<float> left(static_cast<size_t>(hostBuffer)), right(left.size());
                std::vector<double> timings;
                timings.reserve(static_cast<size_t>(duration / hostBuffer + 1));
                double sum = 0;
                int overruns = 0;
                for (int offset = 0; offset < duration; offset += hostBuffer)
                {
                    const int count = std::min(hostBuffer, duration - offset);
                    const float* in[] = {source.data() + offset, source.data() + offset};
                    float* out[] = {left.data(), right.data()};
                    const auto begin = Clock::now();
                    shifter.process(in, out, count);
                    const double us = std::chrono::duration<double, std::micro>(Clock::now() - begin).count();
                    timings.push_back(us); sum += us;
                    if (us > count * 1e6 / rate) ++overruns;
                }
                std::sort(timings.begin(), timings.end());
                csv << candidate.name << ',' << rate << ',' << block << ',' << hop << ',' << config.splitComputation
                    << ',' << latency << ',' << peak << ',' << spectralPeak + 64 << ',' << hostBuffer
                    << ',' << sum / 30000.0 << ',' << timings[static_cast<size_t>(timings.size() * 0.99)]
                    << ',' << timings.back() << ',' << overruns << ',' << width << ',' << centroidMs
                    << ',' << inputBurstWidth << ',' << outputBurstWidth << '\n';
            }
            if (rate == 48000)
            {
                writeWave(folder.getChildFile("synthetic-input.wav"), source, rate);
                for (float pitch : {0.0f, 3.0f, -5.5f, 12.0f, -12.0f})
                    writeWave(folder.getChildFile(juce::String(candidate.name) + "_" + juce::String(pitch, 1) + "st.wav"),
                        shift(source, rate, pitch, config), rate);
            }
        }
    }
    std::cout << "Wrote latency-benchmark CSV measurements and synthetic audition WAVs. No listening assessment performed.\n";
}
