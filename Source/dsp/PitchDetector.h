#pragma once
#include <array>

// Bounded, causal YIN-style detector for a single vocal (65-1100 Hz).
// Storage is fixed; processSample performs no allocation or host calls.
class PitchDetector final
{
public:
    enum class Profile { balanced, fastExperimental };
    struct Result { float hz = 0, confidence = 0; };
    void prepare(double sampleRate, Profile = Profile::balanced) noexcept;
    void reset() noexcept;
    bool processSample(float sample) noexcept; // true on an analysis update
    Result result() const noexcept { return current; }
    double updateSeconds() const noexcept { return double(hopSamples) / analysisRate; }
private:
    void analyse() noexcept;
    static constexpr int capacity = 512;
    int window = 256, hopSamples = 64;
    std::array<float, capacity> ring{}, frame{}, difference{}, normalized{};
    std::array<float, 4> filter{};
    int write = 0, filled = 0, decimation = 4, decimationPosition = 0, hop = 0;
    int minLag = 10, maxLag = 185;
    double analysisRate = 12000;
    float filterCoefficient = 0;
    Result current;
};
