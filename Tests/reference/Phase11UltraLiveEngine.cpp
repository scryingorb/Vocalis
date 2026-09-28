#include "Phase11UltraLiveEngine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void Phase11UltraLiveEngine::prepare(double sr,int count,float initialPitch,float initialFormant,float initialPreserve,double delayMs)
{
    if(!std::isfinite(sr)||sr<44100||sr>96000||(count!=1&&count!=2)||(delayMs!=20&&delayMs!=30))
        throw std::invalid_argument("Unsupported ULTRA experiment preparation");
    rate=sr;channels=count;delay=int(std::ceil(rate*delayMs*.001));
    minimumHz=delayMs==20?150:100;
    // Half a period to choose a nearby epoch + one source period on each side
    // of that epoch. The future output half-grain budget fits the fixed delay.
    lookahead=int(std::floor(delay-1.5*rate/minimumHz))-2;
    capacity=delay*4+16;
    for(auto& b:history)b.assign(size_t(capacity),0);
    for(auto& b:accumulation)b.assign(size_t(capacity),0);
    corrector.prepare(rate,PitchDetector::Profile::fastExperimental);
    tracker.prepare(rate,PitchDetector::Profile::fastExperimental);
    pitch.reset(rate,.02);formant.reset(rate,.02);preserve.reset(rate,.02);voicedMix.reset(rate,.01);
    setSemitones(initialPitch);setFormants(initialFormant,initialPreserve);reset();
}
void Phase11UltraLiveEngine::reset() noexcept
{
    for(auto& b:history)std::fill(b.begin(),b.end(),0.0f);
    for(auto& b:accumulation)std::fill(b.begin(),b.end(),0.0f);
    time=0;nextCentre=lookahead;grains=supported=0;corrector.reset();tracker.reset();
    minimumAge=1e30;maximumAge=0;
    pitch.setCurrentAndTargetValue(pitch.getTargetValue());formant.setCurrentAndTargetValue(formant.getTargetValue());
    preserve.setCurrentAndTargetValue(preserve.getTargetValue());voicedMix.setCurrentAndTargetValue(0);
}
void Phase11UltraLiveEngine::setSemitones(float v) noexcept {pitch.setTargetValue(std::isfinite(v)?std::clamp(v,-12.f,12.f):0);}
void Phase11UltraLiveEngine::setFormants(float v,float p) noexcept
{
    formant.setTargetValue(std::isfinite(v)?std::clamp(v,-6.f,6.f):0);
    preserve.setTargetValue(std::isfinite(p)?std::clamp(p*.01f,0.f,1.f):1);
}
size_t Phase11UltraLiveEngine::slot(int64_t t) const noexcept {return size_t((t%capacity+capacity)%capacity);}
float Phase11UltraLiveEngine::read(int ch,double t) const noexcept
{
    const auto i=int64_t(std::floor(t));
    if(i<0||i+1>time||i<time-capacity+2)return 0;
    const float fraction=float(t-i);const auto& h=history[size_t(ch)];
    return h[slot(i)]+fraction*(h[slot(i+1)]-h[slot(i)]);
}
void Phase11UltraLiveEngine::grain(double centre,double period,double ratio,double formantRatio) noexcept
{
    const double nominal=centre-delay;
    const int radius=int(period*.5);
    // Simple positive excitation-peak proxy. This deliberately does not claim
    // robust glottal closure detection on breathy/noisy material.
    double epoch=nominal;float maximum=-1e30f;
    for(int j=-radius;j<=radius;++j){const double at=nominal+j;const float x=read(0,at);if(x>maximum){maximum=x;epoch=at;}}
    const double half=period/formantRatio;
    const int64_t begin=int64_t(std::ceil(centre-half)),end=int64_t(std::floor(centre+half));
    // Transport-age diagnostics include epoch relocation and grain resampling.
    // They are not the pitch detector's response time or an impulse peak proxy.
    for(double t:{double(begin),double(end)})
    {
        const double age=t-(epoch+(t-centre)*formantRatio);
        minimumAge=std::min(minimumAge,age);maximumAge=std::max(maximumAge,age);
    }
    // Hann pulse overlap gain: hop / half-width. At large downward shifts
    // uncovered/weak regions are possible; these must be evaluated, not hidden.
    const float gain=float(formantRatio/ratio);
    // Peak-centred Hann grains can acquire a DC bias, especially when moving
    // upward by an octave. Remove each grain's weighted mean before synthesis.
    // This is an experimental envelope alteration, not a transparent operation.
    std::array<double,2> mean{};
    double weight=0;
    for(int64_t t=begin;t<=end;++t)
    {
        const double distance=t-centre;
        const double window=.5+.5*std::cos(juce::MathConstants<double>::pi*distance/half);
        weight+=window;
        for(int ch=0;ch<channels;++ch)mean[size_t(ch)]+=read(ch,epoch+distance*formantRatio)*window;
    }
    for(auto& value:mean)value/=std::max(weight,1e-12);
    for(int64_t t=begin;t<=end;++t)
    {
        if(t<time)continue;
        const double distance=t-centre;
        const float window=float(.5+.5*std::cos(juce::MathConstants<double>::pi*distance/half));
        for(int ch=0;ch<channels;++ch)
            accumulation[size_t(ch)][slot(t)]+=(read(ch,epoch+distance*formantRatio)-float(mean[size_t(ch)]))*window*gain;
    }
    ++grains;
}
void Phase11UltraLiveEngine::process(const float* const* input,float* const* output,int samples) noexcept
{
    juce::ScopedNoDenormals guard;
    if(capacity==0)return;
    for(int i=0;i<samples;++i,++time)
    {
        for(int ch=0;ch<channels;++ch)history[size_t(ch)][slot(time)]=std::isfinite(input[ch][i])?input[ch][i]:0;
        const float sample=history[0][slot(time)];tracker.processSample(sample);
        const double correction=corrector.processSample(sample);
        const double semitones=pitch.getNextValue()+correction;
        const double shift=formant.getNextValue(),keep=preserve.getNextValue();
        const double ratio=std::pow(2.,semitones/12),formantRatio=std::pow(2.,(shift+(1-keep)*semitones)/12);
        const auto estimate=tracker.result();
        const double hz=estimate.hz,period=hz>0?rate/hz:0;
        const bool canProcess=hz>=minimumHz&&hz<=1100&&estimate.confidence>=.85f&&period/formantRatio<=lookahead;
        const bool neutral=std::abs(semitones)<1e-6&&std::abs(shift)<1e-6;
        // Unvoiced/unsupported ranges use aligned dry, with an explicit coverage
        // counter in the benchmarks. They must never masquerade as shifted audio.
        voicedMix.setTargetValue(canProcess&&!neutral?1.f:0.f);
        if(canProcess)++supported;
        if(nextCentre-lookahead<=time)
        {
            if(canProcess){grain(nextCentre,period,ratio,formantRatio);nextCentre+=period/ratio;}
            else nextCentre=double(time)+lookahead+1;
        }
        const float mix=voicedMix.getNextValue();
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=time>=delay?history[size_t(ch)][slot(time-delay)]:0;
            auto& wet=accumulation[size_t(ch)][slot(time)];
            output[ch][i]=dry+mix*(wet-dry);wet=0;
        }
    }
}
