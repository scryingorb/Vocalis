#pragma once
#include "../PitchDetector.h"
#include <cmath>

// Research-only temporal safeguard. Confirms an octave-related jump for three
// detector updates; does not guess the missing fundamental or forbid real leaps.
class VocalTrackingGuard final
{
public:
    void reset() noexcept { hz=pending=0; confirmations=missing=acquisition=0; holds=0; }
    void update(PitchDetector::Result result) noexcept
    {
        if(result.hz<=0||result.confidence<.85f)
        {
            acquisition=0;pending=0;confirmations=0;
            if(++missing>=2)hz=0; // bounded hold: at most one analysis hop
            return;
        }
        missing=0;
        if(hz==0)
        {
            if(result.confidence<.90f){acquisition=0;return;}
            if(++acquisition<2)return;
            hz=result.hz;return;
        }
        const double octaveDistance=std::abs(std::log2(result.hz/hz));
        if(std::abs(octaveDistance-1)<.08)
        {
            if(pending==0||std::abs(std::log2(result.hz/pending))>.04){pending=result.hz;confirmations=0;}
            if(++confirmations<3){++holds;return;}
        }
        hz=result.hz;pending=0;confirmations=0;
    }
    float frequency() const noexcept { return hz; }
    unsigned heldOctaveFrames() const noexcept { return holds; }
private:
    float hz=0,pending=0;
    int confirmations=0,missing=0,acquisition=0;
    unsigned holds=0;
};
