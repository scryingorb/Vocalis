# Phase 4: automatic pitch correction

Vocalis 0.4.0 adds monophonic vocal pitch detection and musical-scale correction.
No additional dependency was introduced. Signalsmith and JUCE revisions are unchanged.

## Controls and musical meaning

| Control / stable ID | Range | Default | Meaning |
| --- | --- | --- | --- |
| Key / `key` | C, C#, D, D#, E, F, F#, G, G#, A, A#, B | C | Tonic; ignored for Chromatic |
| Scale / `scale` | Major, Minor, Chromatic | Chromatic | Minor means natural minor |
| Correction Amount / `correctionAmount` | 0-100% | 0% | Fraction of calculated correction; 0 disables detection/correction |
| Retune Speed / `retuneSpeed` | 0-100% | 50% | Higher = faster correction; exponential time constant 250 ms down to 4 ms |
| Note Transition / `noteTransition` | 0-100% | 50% | Higher = longer confirmation before accepting a different target |
| Natural / Hard Tune / `hardTune` | 0-100% | 0% | Continuous blend from pitch-centre correction to instantaneous pitch correction |

Reference tuning is A4 = 440 Hz, twelve-tone equal temperament. Target search uses
all octaves, including adjacent octaves at B/C boundaries. Exact equal-distance
ties favour the lower allowed note. Major intervals: 0,2,4,5,7,9,11; natural
minor: 0,2,3,5,7,8,10. The scale selection function is separate from detection
so additional scales can be added later without changing the detector.

Amount 100 does not by itself mean instantaneous hard tuning. At Natural 0,
a 120 ms pitch-centre estimator removes slower tuning error and leaves much of
faster vibrato. At Hard 100 the correction reference is instantaneous detected
pitch. Retune Speed still limits how rapidly the correction follows its target;
100% Hard with slow Retune will not flatten all vibrato. The shortest smoothing
time remains 4 ms to avoid discontinuous transposition.

Target selection uses detected pitch with hysteresis and confirmation time,
independently of the slower natural pitch centre. Hysteresis has a 0.3-semitone
distance advantage at Natural and decreases to zero at Hard. Transition holds a
candidate for 0-100 ms at Natural, or 0-10 ms at Hard. A confirmed note change
resets the centre to the new detected pitch, avoiding a long pull toward the
previous note. Detection/analysis response is additional to these control times.

Correction is limited to +/-3 semitones. Manual Pitch is added *after* selecting
the input note's correction: tuning a vocal to C major and adding manual +3
transposes that corrected scale by three semitones. Manual Pitch still has its
existing +/-12 range. Formant Preserve follows the combined transposition.

## DSP and realtime behavior

- `PitchDetector.h/.cpp`: original bounded YIN-style implementation based on the
  [published difference-function method](https://pubmed.ncbi.nlm.nih.gov/12002874/).
  Four cascaded low-pass stages precede integer decimation to at most 12 kHz.
  A fixed-size ring stores samples. A 256-sample comparison window and lags for
  65-1100 Hz produce a cumulative mean normalized difference function every
  64 decimated samples. The first sufficiently periodic valley is refined by
  parabolic interpolation on the raw squared difference function.
- Confidence must be at least 0.85, with two voiced updates for acquisition.
  Silence/low periodicity releases the correction toward zero over a 25 ms
  time constant. The filtered RMS gate is approximately -70 dBFS; it is not a
  noise suppressor. No pitch is displayed when the detector is unvoiced.
- `PitchCorrector.h/.cpp`: scale selection, target hysteresis, note confirmation,
  natural pitch-centre tracking, amount scaling and sample-wise retune smoothing.
- `PitchShifter`: feeds detector from the pre-shift wet input and adds correction
  to the manual pitch trajectory, retaining the same Signalsmith spectral pass.
  Stereo uses the **left channel as its shared vocal detection reference**;
  both channels receive the same correction. Use a mono vocal or a stereo vocal
  with the lead present on the left. A right-only vocal must be routed to mono
  first. This avoids cancellation when stereo channels are opposite polarity.
- The detector describes earlier samples, so correction is not sent through the
  complete manual automation history. A control-only ring accounts for analysis
  lookback beyond an approximate 20 ms detector age (especially QUALITY's larger
  window). Exact age varies with frequency and hop position; this is causal
  correction, not exact sample-tagged lookahead or zero-delay note acquisition.
- APVTS parameters are cached atomic reads. Detection, mapping and smoothing use
  fixed/prepared storage; no allocation, locks, logging, file/network access or
  UI calls occur in the processing path. A message-thread timer reads an atomic
  frequency for the Pitch Orb. Detection is skipped at Amount 0.

All six new parameters automate and serialize with APVTS. Missing parameters in
older projects restore to the defaults above, notably Amount 0. Existing IDs
and the Phase 3 legacy formant-state behavior are unchanged.

## Latency and limitations

There is no new audio delay stage. Reported LIVE latency remains:

| Rate | Samples | Milliseconds |
| --- | ---: | ---: |
| 44.1 kHz | 2,180 | 49.43 |
| 48 kHz | 2,368 | 49.33 |
| 96 kHz | 4,672 | 48.67 |

QUALITY remains internal at approximately 121 ms. Dry/wet and bypass use the
same existing delay. Correction acquisition takes roughly 40-50 ms on steady
probes, plus parameter smoothing and spectral response. Plugin delay compensation
aligns tracks; it cannot eliminate microphone monitoring delay.

This is a monophonic vocal corrector, not a polyphonic/chord detector. Breathy
consonants, noise, overlapping voices, reverb and very fast glides can cause
tracking loss or octave errors. It does not identify the song's key: the user
must choose it. Natural vibrato separation is a smoothing approximation, not
an analysis of expressive intent. Hard Tune is limited by causal detection and
the existing LIVE spectral engine's accuracy, not guaranteed mathematically
exact tuning. Existing formant reconstruction and stereo-image limitations remain.

No real-vocal listening session or interactive FL Studio test was performed here.
Synthetic tests verify behavior and regressions but do not establish commercial
vocal quality or guarantee absence of audible artifacts on every voice.

## Validation and reproducibility

Build and run the complete suite using README's Release commands. The focused
correction checks can also be run from `build/`:

```powershell
& './VocalisTests_artefacts/Release/VocalisTests.exe' --correction-tests
```

Coverage includes all twelve keys and three scales; sine/harmonic and missing-
fundamental probes at 75/110/220/440/880 Hz; silence and noise rejection; amount
0/50/100; retune response; note-transition response; natural/hard vibrato in both
control and rendered audio; additive manual pitch; retained QUALITY operation;
mono/stereo at 44.1/48/96 kHz; variable buffers; exact buffer-partition equality;
state round-trip and migration; rapid automation; and tested C++ allocation safety.
The complete suite also retains the pitch/formant, unity, dry/wet, bypass, reset,
UI rendering, and generated-VST3 discovery/load checks from previous phases.

The CPU benchmark times the **complete processor callback**, using a pre-rendered
225 Hz tone plus its second harmonic, stereo, three seconds per case. It compares
Amount 0 against Amount 100 / Hard 100 / Speed 100 at each rate with 64/256-sample
buffers. Preparation, source generation and CSV output are outside timing.
These measurements are internally comparable; unlike the Phase 3 benchmark,
they include the processor's gain/mix/delay work and use a different signal.
Percentages are processing time / audio duration, not Windows total CPU. Deadline
counts are offline callback times over the sample budget, not observed ASIO errors.

## FL Studio listening check

Install the complete new VST3 bundle following README and rescan the updated
plugin. Use a dry, single vocal, gains 0 dB, Wet 100%, manual Pitch/Formant 0 and
Formant Preserve 100. Select the song's actual key and Major or natural Minor;
use Chromatic when testing against a semitone reference.

- Natural starting point: Amount 70%, Speed 40%, Transition 60%, Hard 0%.
- Stronger correction: Amount 100%, Speed 70%, Transition 30%, Hard 30%.
- Hard effect: Amount 100%, Speed 100%, Transition 0%, Hard 100%.

Listen to sustained slightly mistuned notes, vibrato, slides, note changes and
consonants. Compare Amount 0 to disable correction. Automate the controls, save
and reopen the project, and confirm settings return. The Pitch Orb displays
**detected input** pitch, not the corrected output or selected scale target.

## Measured Release results

The complete CTest suite passed in 73.73 seconds on the i5-13600K machine.
Release compilation completed without compiler warnings in the final build log.
The generated VST3 was discovered and loaded by the JUCE test host; FL Studio
itself was not used for this automated run.

- Detector maximum error: **1.88 cents** across the tested voiced probes,
  including missing fundamentals. Noise: **0 / 375** voiced analysis updates.
- Listed rendered correction cases: maximum absolute error **6.02 cents**.
  The residual includes the existing short-window LIVE pitch shifter's error.
- A 5 Hz, +/-22-cent synthetic vibrato probe: rendered 5th-to-95th-percentile
  pitch span **40.19 cents Natural**, **6.99 cents Hard**, both with Speed 100.
  This verifies audible-path modulation behavior on a synthetic tone, not a
  claim that all singers' vibrato is perfectly preserved or eliminated.
- Natural-mode target transition probe: low/high Transition changed targets
  after **23.98 / 114.65 ms** from the input note change. These measured values
  include detection response and differ from the nominal confirmation times.
- No tested C++ callback allocations, nonfinite/runaway outputs, stereo clock
  mismatch, host-buffer partition dependence or automation discontinuity failures.
- LIVE and QUALITY reported latency remained unchanged; all earlier regression
  tests passed, including Phase 3's documented small linked-QUALITY differences.

Full-processor CPU at 48 kHz, stereo:

| Buffer | Amount 0 | Amount 100 / Hard 100 | Active p99 | Active max | Misses |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 | 4.36% | 6.39% | 242.4 us | 950.5 us | 0 |
| 256 | 4.40% | 6.34% | 608.3 us | 871.1 us | 0 |

At 96 kHz/64, two offline deadline overruns occurred with correction disabled
and two with it enabled. No misses occurred at 256 samples at any tested rate.
Do not interpret this short benchmark as a guarantee of ASIO reliability.

Evidence:
[rendered pitch](measurements/phase4-pitch.csv),
[CPU and deadlines](measurements/phase4-cpu.csv),
[complete suite output](measurements/phase4-tests.log).

New source files: `Source/dsp/PitchDetector.h/.cpp`,
`Source/dsp/PitchCorrector.h/.cpp`, `Tests/CorrectionTests.cpp`.
Integration changes: `Source/ParameterIDs.h`, `Source/dsp/PitchShifter.h/.cpp`,
`Source/PluginProcessor.h/.cpp`, `Source/PluginEditor.h/.cpp`,
`Source/ui/PitchOrb.h/.cpp`, `Tests/ProcessorTests.cpp`, `CMakeLists.txt`, README,
this guide and the measurement archives. Third-party notices are unchanged.

Release bundle (copy the entire directory when installing):

```text
C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3
```
