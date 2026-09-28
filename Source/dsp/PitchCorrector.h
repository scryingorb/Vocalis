#pragma once
#include "PitchDetector.h"
#include "Humanizer.h"

class PitchCorrector final
{
public:
    // Scale: 0 major, 1 natural minor, 2 chromatic. Key: 0 C ... 11 B.
    struct Controls { int key = 0, scale = 2; float amount = 0, speed = 50, transition = 50, hard = 0; };
    void prepare(double sampleRate, PitchDetector::Profile = PitchDetector::Profile::balanced) noexcept;
    void reset() noexcept;
    void setHumanizeControls(Humanizer::Controls c) noexcept { humanizer.setControls(c); }
    void setControls(Controls) noexcept;
    float processSample(float input) noexcept;
    float detectedHz() const noexcept { return detector.result().hz; }
    int targetNote() const noexcept { return target; }
    float correctionSemitones() const noexcept { return correction; }
    static bool allowed(int midiNote, int key, int scale) noexcept;
    static int nearestNote(float midiNote, int key, int scale) noexcept;
private:
    void update() noexcept;
    void updateRetuneCoefficient() noexcept;
    Humanizer humanizer;
    double baseRetuneSeconds = 0.25;
    PitchDetector detector;
    Controls controls;
    double rate = 48000;
    float correction = 0, desired = 0, centre = 0;
    float retuneCoefficient = 0, releaseCoefficient = 0;
    int target = -1, candidate = -1, candidateFrames = 0, voicedFrames = 0;
    bool tracking = false;
};
