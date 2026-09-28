#include "UltraLiveEngine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void UltraLiveEngine::prepare(double sr,int count,float initialPitch,float initialFormant,float initialPreserve,double delayMs)
{
    if(!std::isfinite(sr)||sr<44100||sr>96000||(count!=1&&count!=2)||(delayMs!=20&&delayMs!=30))
        throw std::invalid_argument("Unsupported ULTRA experiment preparation");
    rate=sr;channels=count;delay=int(std::ceil(rate*delayMs*.001));
    minimumHz=delayMs==20?150:75;
    // Half a period to choose a nearby epoch + one source period on each side
    // of that epoch. The future output half-grain budget fits the fixed delay.
    lookahead=int(std::floor(delay-1.5*rate/(delayMs==20?150:100)))-2;
    capacity=delay*4+16;
    for(auto& b:history)b.assign(size_t(capacity),0);
    for(auto& b:accumulation)b.assign(size_t(capacity),0);
    overlapWeight.assign(size_t(capacity),0);
    voicingHistory.assign(size_t(capacity),0);
    differenceLag=std::max(1,int(std::round(rate/12000)));
    energyCoefficient=float(1-std::exp(-1/(rate*.002)));
    corrector.prepare(rate,PitchDetector::Profile::fastExperimental);
    tracker.prepare(rate,PitchDetector::Profile::fastExperimental);
    pitch.reset(rate,.02);formant.reset(rate,.02);preserve.reset(rate,.02);voicedMix.reset(rate,.01);
    setSemitones(initialPitch);setFormants(initialFormant,initialPreserve);reset();
}
void UltraLiveEngine::reset() noexcept
{
    for(auto& b:history)std::fill(b.begin(),b.end(),0.0f);
    for(auto& b:accumulation)std::fill(b.begin(),b.end(),0.0f);
    time=0;nextCentre=lookahead;grains=supported=0;corrector.reset();tracker.reset();
    minimumAge=1e30;maximumAge=0;
    std::fill(overlapWeight.begin(),overlapWeight.end(),0.0f);
    std::fill(voicingHistory.begin(),voicingHistory.end(),0.0f);
    trackingGuard.reset();epochValid=false;lastEpoch=lastPeriod=0;energy=differenceEnergy=0;
    pitch.setCurrentAndTargetValue(pitch.getTargetValue());formant.setCurrentAndTargetValue(formant.getTargetValue());
    preserve.setCurrentAndTargetValue(preserve.getTargetValue());voicedMix.setCurrentAndTargetValue(0);
}
void UltraLiveEngine::setSemitones(float v) noexcept {pitch.setTargetValue(std::isfinite(v)?std::clamp(v,-12.f,12.f):0);}
void UltraLiveEngine::setFormants(float v,float p) noexcept
{
    formant.setTargetValue(std::isfinite(v)?std::clamp(v,-6.f,6.f):0);
    preserve.setTargetValue(std::isfinite(p)?std::clamp(p*.01f,0.f,1.f):1);
}
size_t UltraLiveEngine::slot(int64_t t) const noexcept {return size_t((t%capacity+capacity)%capacity);}
float UltraLiveEngine::read(int ch,double t) const noexcept
{
    const auto i=int64_t(std::floor(t));
    if(i<0||i+1>time||i<time-capacity+2)return 0;
    const float fraction=float(t-i);const auto& h=history[size_t(ch)];
    return h[slot(i)]+fraction*(h[slot(i+1)]-h[slot(i)]);
}
void UltraLiveEngine::grain(double centre,double period,double ratio,double formantRatio) noexcept
{
    const double nominal=centre-delay;
    const int radius=int(period*.5);
    // Bound the input half-grain to samples actually available when scheduled.
    // At low F0 this shortens the window, not the pitch period; latency stays fixed.
    const double sourceHalf=std::min(period,double(delay-lookahead-radius-3));
    if(sourceHalf<=2)return;
    double prediction=nominal;
    const bool continuous=epochValid&&std::abs(period-lastPeriod)<period*.25;
    if(continuous)prediction=lastEpoch+std::round((nominal-lastEpoch)/period)*period;
    // Follow the prior pulse near its predicted phase rather than independently
    // selecting the loudest secondary peak on every grain. Reacquire on large leaps.
    double epoch=nominal;float maximum=-1e30f;
    const double left=std::max(nominal-radius,prediction-(continuous?.20:.50)*period);
    const double right=std::min(nominal+radius,prediction+(continuous?.20:.50)*period);
    for(double at=std::ceil(left);at<=right;++at){const float x=read(0,at);if(x>maximum){maximum=x;epoch=at;}}
    const double a=read(0,epoch-1),b=read(0,epoch),c=read(0,epoch+1),curve=a-2*b+c;
    if(curve< -1e-12)epoch+=std::clamp(.5*(a-c)/curve,-.5,.5);
    epoch=std::clamp(epoch,nominal-radius,nominal+radius);
    lastEpoch=epoch;lastPeriod=period;epochValid=true;
    const double half=sourceHalf/formantRatio;
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
    const float gain=float(period/(ratio*half));
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
        overlapWeight[slot(t)]+=window*gain;
        for(int ch=0;ch<channels;++ch)
            accumulation[size_t(ch)][slot(t)]+=(read(ch,epoch+distance*formantRatio)-float(mean[size_t(ch)]))*window*gain;
    }
    ++grains;
}
void UltraLiveEngine::process(const float* const* input,float* const* output,int samples) noexcept
{
    juce::ScopedNoDenormals guard;
    if(capacity==0)return;
    for(int i=0;i<samples;++i,++time)
    {
        for(int ch=0;ch<channels;++ch)history[size_t(ch)][slot(time)]=std::isfinite(input[ch][i])?input[ch][i]:0;
        const float sample=history[0][slot(time)];
        if(tracker.processSample(sample))trackingGuard.update(tracker.result());
        const float difference=sample-(time>=differenceLag?history[0][slot(time-differenceLag)]:0);
        energy+=energyCoefficient*(sample*sample-energy);
        differenceEnergy+=energyCoefficient*(difference*difference-differenceEnergy);
        // Fast noise/fricative veto, evaluated at the corresponding delayed input
        // time. Confidence remains the primary periodicity test. This is not a
        // phoneme recognizer; mixed breath/voice remains a documented compromise.
        voicingHistory[slot(time)]=energy>1e-9f&&differenceEnergy<1.15f*energy?1.f:0.f;
        const double correction=corrector.processSample(sample);
        const double semitones=pitch.getNextValue()+correction;
        const double shift=formant.getNextValue(),keep=preserve.getNextValue();
        const double ratio=std::pow(2.,semitones/12),formantRatio=std::pow(2.,(shift+(1-keep)*semitones)/12);
        const double hz=trackingGuard.frequency(),period=hz>0?rate/hz:0;
        if(hz==0)epochValid=false;
        const double sourceHalf=std::min(period,double(delay-lookahead-int(period*.5)-3));
        const bool canProcess=hz>=minimumHz&&hz<=1100&&sourceHalf>2&&sourceHalf/formantRatio<=lookahead;
        const bool neutral=std::abs(semitones)<1e-6&&std::abs(shift)<1e-6;
        // Unvoiced/unsupported ranges use aligned dry, with an explicit coverage
        // counter in the benchmarks. They must never masquerade as shifted audio.
        if(canProcess)++supported;
        if(nextCentre-lookahead<=time)
        {
            if(canProcess){grain(nextCentre,period,ratio,formantRatio);nextCentre+=period/ratio;}
            else nextCentre=double(time)+lookahead+1;
        }
        const float weight=overlapWeight[slot(time)];
        const bool voicedAtOutput=time>=delay&&voicingHistory[slot(time-delay)]>0;
        // Do not fade toward wet before scheduled grains exist. Normalizing the
        // partition of unity removes grain-overlap pumping, without peak limiting.
        voicedMix.setTargetValue(weight>.15f&&voicedAtOutput&&!neutral?1.f:0.f);
        const float mix=voicedMix.getNextValue();
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=time>=delay?history[size_t(ch)][slot(time-delay)]:0;
            auto& wet=accumulation[size_t(ch)][slot(time)];
            output[ch][i]=dry+mix*(wet/std::max(weight,.5f)-dry);wet=0;
        }
        overlapWeight[slot(time)]=0;
    }
}
