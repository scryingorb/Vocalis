# Phase 11: low-latency engineering evaluation

> Phase 12 follow-up: [UltraLiveValidation.md](UltraLiveValidation.md) records
> revised ULTRA30 measurements and blind auditions. Its tested lower range now
> reaches 80 Hz (75 Hz eligibility floor), superseding the Phase 11 100 Hz limit
> below. Fixed scheduling remains 30 ms. Persistent octave ambiguity prevents
> Beta promotion; production LIVE/QUALITY are unchanged. The historical Phase 11
> measurements below remain intact for comparison.

## Release decision

**ULTRA LIVE was implemented and tested, but is not ready for release.** The
normal VST3 retains the known-good approximately 49 ms LIVE engine, 18 parameter
IDs, preset values, state format and UI. No processing-mode selector was added.
QUALITY remains available through the existing preparation-only DSP API.

The new `Source/dsp/live/UltraLiveEngine.*` is linked into the research/test
executable only. Two candidates use 30 ms and 20 ms fixed scheduling buffers.
They must not be advertised as production plugin latency or full-range vocal
processing. The 20 ms candidate cannot process low-register input, and both
candidates substantially attenuate an upward-octave pure-tone test. Those are
release blockers, even though moderate shifts and CPU results are promising.

No new dependency was added. Signalsmith Stretch/Linear and the stable spectral
configuration remain intact. The only shared DSP change is an opt-in detector
profile, defaulting to the original settings; PitchCorrector forwards that option.
The original 0.9.0 ZIP remains untouched in dist/. A pre-change VST3 and source
hash manifest were saved under build/phase11-baseline/.

## Architecture investigation and choice

The existing short-window Signalsmith approach was not shortened again. It
already trades low-register accuracy against temporal resolution. A monophonic
time-domain experiment lets us evaluate a different compromise while retaining
Signalsmith for established LIVE/QUALITY processing.

The candidate uses TD-PSOLA-style Hann grains centred on a nearby positive
waveform peak (an excitation proxy), rescheduled at the corrected fundamental.
Resampling within a grain changes the envelope/formant character independently
of the synthesis spacing. Shared PitchCorrector still selects key/scale targets;
there is no duplicate music-theory system. Humanizer shapes its trajectories;
BloomMapping feeds the same humanization/formant controls without audio buffering.
Manual pitch, formant and preserve controls have 20 ms ramps in the experiment.

This is an internal research implementation, not a claim to reproduce a published
PSOLA/ESOLA implementation. Positive peaks are not robust glottal-closure instants.
Breath, changing vowels, low notes and transient boundaries can destabilize epoch
selection. Removing the window-weighted mean of each grain avoids DC buildup,
but changes the envelope and exposes a severe cancellation at the octave-up
pure-tone case. An LPC/residual or better epoch model would require a separate
quality investigation; neither was silently substituted here.

Alternatives reviewed:

- [ESOLA research](https://arxiv.org/abs/1801.06492) motivates epoch alignment but
  does not establish a drop-in, fixed 10 ms, realtime Vocalis solution.
- [Signalsmith Stretch](https://signalsmith-audio.com/code/stretch/) remains the
  existing open-source spectral implementation.
- [Signalsmith Compose](https://compose.signalsmith-audio.co.uk/) advertises a
  5-10 ms typical configuration. Its vendor identifies it as proprietary on the
  [licensing page](https://signalsmith-audio.com/code/). It was not acquired,
  integrated or benchmarked; advertised performance is not our measurement.

## Fixed buffers and actual timing

No hidden block adapter is used in ULTRA: it consumes and emits one sample at a
time. All vectors are allocated during prepare. The input ring and future-grain
accumulation include the complete fixed scheduling delay shown below. No realtime
latency changes, buffer reconfiguration, locks or UI operations occur.

| Rate | ULTRA20 buffer / ms | ULTRA30 buffer / ms | LIVE reported / ms | QUALITY DSP / ms |
| --- | --- | --- | --- | --- |
| 44.1 kHz | 882 / 20 | 1323 / 30 | 2180 / 49.4331 | 5356 / 121.4512 |
| 48 kHz | 960 / 20 | 1440 / 30 | 2368 / 49.3333 | 5824 / 121.3333 |
| 88.2 kHz | 1764 / 20 | 2646 / 30 | 4297 / 48.7188 | 10648 / 120.7256 |
| 96 kHz | 1920 / 20 | 2880 / 30 | 4672 / 48.6667 | 11584 / 120.6667 |

These ULTRA numbers are **fixed scheduling/neutral dry delays**, not proof that
all voiced features emerge at exactly that offset. Pitch-synchronous relocation
moves features by an epoch fraction. With 220 Hz input, +3 st and Preserve 100%,
measured source-sample transport ages span approximately 27.7-32.3 ms for ULTRA30
and 17.7-22.3 ms for ULTRA20. Formant resampling can widen this range. The diagnostic
includes epoch relocation and intra-grain resampling; there is no extra adapter
buffer omitted from the table.

Neutral impulses verify the exact fixed delay but **do not qualify as active
pitch-shifting latency measurements**. A separate voiced-carrier/tiny-marker
probe requires nonzero synthesis-grain counts. Its raw earliest difference,
peak, energy centroid and tail are archived. Some rates produce a lasting phase
change from the marker; their difference peaks/centroids cannot be interpreted
as a single latency. We do not label a 500 ms difference tail as 500 ms buffering,
nor substitute the clean-path impulse result for an active-processing result.
This transient sensitivity requires further work before host integration.

ULTRA30 currently requires input F0 >=100 Hz; ULTRA20 requires >=150 Hz. A grain
that cannot fit its fixed lookahead (e.g. some low/pitch/formant/preserve
combinations) is explicitly unsupported and crossfades to aligned dry. Coverage
counters identify this: the 123 Hz CPU study has **zero processed grains for
ULTRA20**. Its cheap fallback results must not be used as active-engine CPU wins.
The separate 220 Hz CPU study exercises both candidates with real synthesis.

Adaptive windows were considered. Rather than changing host delay with every
note, the experiment keeps a fixed maximum scheduling budget and varies grain
length within it. Widening that budget enough for deep voices and all envelope
settings would erode the latency advantage. Unsupported audio is a research
limitation, not a hidden promise of successful pitch shifting.

## Detector and correction response

Balanced YIN-style detector: decimate to <=12 kHz using four cascaded first-order
low-pass stages; 256-sample comparison window, 64-sample hop, lag search for
65-1100 Hz, normalized-difference threshold 0.15 (confidence >=0.85). It needs
window + maximum lag + 1 decimated samples before its first analysis. At 48 kHz
this is 442 samples at 12 kHz (36.83 ms) plus hop quantization; at 44.1 kHz the
analysis rate is 11025 Hz. Filter settling and periodicity affect real inputs.

Experimental profile: 128-sample comparison window, 32-sample hop; same lag range,
filter and confidence threshold. It does not pretend a 65 Hz period can be
estimated instantaneously. This profile is opt-in and not used by released LIVE.

Measured harmonic-tone cold acquisition across all four rates and 65-880 Hz:

| Detector | First correct estimate | Mean absolute steady error per case |
| --- | --- | --- |
| Balanced | 37.31-40.62 ms | 0.017-1.18 cents |
| Fast experimental | 26.65-29.01 ms | 0.026-1.18 cents |

No wrong/unvoiced steady frames occurred in this clean deterministic set. This
is not evidence of equivalent robustness on breathy or noisy vocals.

The shared corrector requires two voiced frames for acquisition. Natural mode
tracks note centre with a 120 ms time constant, includes boundary hysteresis, and
retains vibrato. Retune ranges from 250 to 4 ms; release is 25 ms. Note Transition
adds target persistence up to 100 ms in natural mode, reduced by hard tuning.
Humanize can intentionally slow response; it uses past measurements and adds no
audio buffer. Hard Tune bypasses centre tracking in its correction reference;
it still has confidence/acquisition rules and the nonzero retune time constant.

A 201 -> 239 Hz synthetic harmonic step with Amount/Speed/Hard at 100%, Transition
0, Humanize/Bloom 0 measures target selection separately from output settling.
See response.csv for all rates. At 48 kHz, the initial run found:

| Mode | Input estimate near 239 Hz | Target first reaches A#3 | Output settling observation | Target-to-output observation |
| --- | --- | --- | --- | --- |
| LIVE | 23.31 ms | 17.98 ms | 73.98 ms | 56.00 ms |
| QUALITY | 23.31 ms | 17.98 ms | 148.65 ms | 130.67 ms |
| ULTRA30 | 15.31 ms | 9.98 ms | about 50 ms | about 40 ms |
| ULTRA20 | 15.31 ms | 9.98 ms | about 39 ms | about 29 ms |

Target selection can precede the tightly converged input estimate. Output settling
means three consecutive independent fast-detector observations within 20 cents
of the destination; that observer adds analysis delay. These are estimates with
measurement-window overhead, not a sample-exact acoustic onset or microphone
round-trip latency. Final response.csv contains results after the grain-DC fix.

## Pitch, formants and release gates

The study covers 0, +/-1, +/-3, +/-7 and +/-12 st at 80/110/220/440 Hz across all
four rates. Both zero crossings and an independent output detector are recorded.
Zero crossings are **not** a reliable fundamental estimator for PSOLA's altered
waveform. For example, -7 st can have a correct detected fundamental while its
zero-crossing count differs substantially. Detector sentinel 9999 means no
estimate; expected frequencies below 65 or above 1100 Hz are outside that
observer's range and must not be scored as ordinary cents errors.

At 48 kHz / 220 Hz, ULTRA30's output-detector errors were approximately -0.53 cents
at +3 st and -0.12 cents at -3 st. However, +12 st reduced RMS from 0.05657 to
0.00007405 (about **-57.7 dB**), with no detected fundamental. This is a major
failure, not merely an acceptable change in vocal character. ULTRA20 also fails
this octave-up case. Low-register unsupported samples stay unshifted, so their
reported pitch errors must not be omitted from coverage discussion.

The 120 Hz synthetic vowel first-band centroid responds to ULTRA30 formant
shifts (roughly 607 / 702 / 814 Hz at -3 / 0 / +3 st in the initial study).
ULTRA20 leaves it unchanged because that source is unsupported. Final formants.csv
records the post-fix values. Centroid movement demonstrates an actual envelope
operation, not EQ masquerading as formant shifting; it does not establish natural
vowel quality. Full independent formant quality and Preserve coverage are not
sufficient for release.

Humanize and Bloom reuse the existing control logic. All nine preset value arrays
are unchanged and exercised against the research engine for safety. Unsupported
range/fallback and envelope limitations mean their **sound is not equivalent** in
ULTRA; they remain fully functional in the production LIVE plugin.

## CPU methodology and evidence

Baseline was completed before DSP edits at 44.1/48/88.2/96 kHz, mono/stereo and
32/64/128/256/512 buffers. Timing is offline wall-clock engine processing,
including detector/corrector/humanizer and formants, excluding signal synthesis,
UI, host scheduling and the processor's gain/mix wrapper. It is not total machine
CPU. Average cost, worst observed callback and actual callback-deadline misses
are recorded. One-second cases and observed maxima are not worst-case guarantees.
No parallel build/test load was intentionally run during timing measurements.

The archived original baseline, initial candidates and final comparison remain
separate. See [CPU tables](measurements/phase11/CPUComparison.md) for the matched
220 Hz active-engine comparison. The 123 Hz main study exposes low-register
fallback and cannot support an active ULTRA20 performance claim.

Prior final-study 123 Hz results included LIVE misses at 96k/64 and QUALITY misses
at 48k/32 and 96k/64/128. Low average CPU does not guarantee a tiny-buffer deadline:
QUALITY performs larger bursts. Do not label these configurations universally
live-safe. No subjective listening observations are claimed here.

## Tests, reproducibility and mode lifecycle

Commands from the build directory after building Release VocalisTests:

```powershell
./VocalisTests_artefacts/Release/VocalisTests.exe --live-study phase11-final-study
./VocalisTests_artefacts/Release/VocalisTests.exe --active-cpu
./VocalisTests_artefacts/Release/VocalisTests.exe --ultra-tests
./VocalisTests_artefacts/Release/VocalisTests.exe --live-auditions
```

The complete CTest suite also runs ULTRA finite-output, C++ allocation, mono/stereo,
variable-buffer, reset, neutral-delay and all-preset control tests, plus two minutes
of simulated pitch/formant/retune/Humanize/Bloom automation and repeated stopped
20/30 ms re-preparation. No claims of multi-hour real-device endurance are made.
The established suite covers host VST3 loading, state/automation, dry/wet/bypass,
all presets and LIVE/QUALITY regressions. Historical Phase 7 sample-exact tests
verify that the opt-in detector refactoring does not change balanced output.

There is intentionally no realtime host mode switch, mode APVTS parameter or
saved-state schema change. An unready algorithm should not require existing
projects to migrate. Future promotion must prepare a selected mode while the host
is stopped/suspended, reset histories and dry delay, then call setLatencySamples
once before resumed processing. It must not reconfigure or repeatedly update
latency in processBlock. Creative presets should remain independent of that
processing choice. Until those integration tests are possible, ULTRA dry/wet,
host bypass and FL Studio mode-switch recognition are **not validated**; the
production aligned paths remain unchanged and tested.

## Listening and FL Studio instructions

Because the candidate failed release gates, this VST3 has **no ULTRA selector**.
It would be misleading to instruct you to select one or claim that FL Studio has
recognized a new latency. The production VST3 remains at:

`C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3`

For the stable plugin: close FL Studio, copy the entire outer bundle to
`C:\Program Files\Common Files\VST3`, then rescan in Plugin Manager. Put it on a
microphone Mixer track, use the audio interface's native ASIO driver, and start
at 48 kHz / 128 or 256 samples. Monitor only the intended return (avoid a second
direct-monitor path), keep delay compensation enabled, and use headphones to
avoid feedback. Set the song Key/Scale. Compare Natural versus Hard Tune presets,
then moderate Pitch +/-3 st and Formant +/-2 st. Save/reopen the project and verify
that earlier automation/presets still work. Actual FL Studio interaction was not
performed in this environment.

For candidate listening, import build/phase11-audition/*.wav into separate FL
Studio Playlist tracks, route them to equivalent mixer paths and solo one at a
time. The source is an 8-second deterministic synthetic sequence:

1. 0-1 s: sustained 120 Hz vowel (ULTRA20 unsupported).
2. 1-2 s: 220 Hz vibrato.
3. 2-3 s: 110-330 Hz slide.
4. 3-4 s: fast alternating notes.
5. 4-5 s: 80 Hz low voice (both candidates unsupported).
6. 5-6 s: 440 Hz higher vowel.
7. 6-7 s: breath/noise mixed with a vowel.
8. 7-8 s: synthetic plosive/noise/sibilance proxy.

The +3 files use Formant 0; the -3 files use Formant +2; Preserve is 100%.
Compare only files with the same settings. For timing comparison retain their
native offsets; for timbre comparison offset each by its nominal mode delay and
match perceived level without hiding dropouts. Listen for graininess, doubling,
flutter, phasing, metal-like artifacts, smearing and unstable formants. These
fixtures do not replace dry recordings of real singers and speech. Rebuild/run
the fixture tool as above to regenerate them; no recorded voice was supplied or
auditioned. Future microphone ULTRA/LIVE/QUALITY A/B testing must wait for a
candidate that passes quality and host-integration gates.

Total monitoring delay includes interface input conversion, ASIO/FL Studio input
and output buffering, plugin processing and interface output conversion. The
plugin's delay is not total round-trip latency. Stable LIVE remains the safest
default for this prototype; this phase does not claim comfortable live monitoring
has been achieved.

## Final mode comparison and decision record

CPU below is the matched 220 Hz stereo study at 48 kHz / 64 samples. Values are
mean / maximum observed microseconds per callback, not a universal CPU guarantee.

| Mode | Algorithm | Delay at 48 kHz | CPU us | Pitch result | Formant result | Intended use / status |
| --- | --- | --- | --- | --- | --- | --- |
| ULTRA20 | Experimental TD-PSOLA | 960-sample scheduling / 20 ms; epoch movement additional | 17.03 / 158.9 | Moderate shifts accurate on supported tones; octave-up collapse; <150 Hz unsupported | 120 Hz vowel falls back unchanged | High-voice research only, rejected for release |
| ULTRA30 | Experimental TD-PSOLA | 1440-sample scheduling / 30 ms; epoch movement additional | 16.79 / 45.6 | +/-3 st within 1 cent at 220 Hz; octave-up collapse; <100 Hz unsupported | Measured envelope shift; limited coverage/naturalness unverified | Broader monophonic research, rejected for release |
| LIVE | Signalsmith Stretch, 48 ms window, 64-sample adapter | 2368 / 49.33 ms reported | 111.39 / 334.9 | Established regression tolerances retained | Established spectral envelope path | Recommended production default; still noticeable monitoring delay |
| QUALITY | Signalsmith Stretch, 120 ms window, 64-sample adapter | 5824 / 121.33 ms DSP delay | 45.32 / 528.6 | Established higher-window option retained | Existing spectral envelope path | Internal playback/quality option; unsuitable for low-delay monitoring |

No subjective listening claims are attached to this table. QUALITY's lower mean
cost reflects its larger hop / fewer transforms, but its callback bursts are
larger. Across all 20 matched-input rate/buffer cases per mode (16738 callbacks),
observed deadline misses were LIVE 650, QUALITY 256, ULTRA30 0, ULTRA20 0. Most
LIVE misses occurred at 88.2/96 kHz with 32-sample buffers; QUALITY also repeatedly
missed at 64 samples at those rates. A 96 kHz / 512 LIVE callback had a 9.57 ms
outlier, demonstrating why short offline timings are not an ASIO certification.
All four had zero misses in the 48 kHz / 64 case above.

Full Release CTest passed: 100%, zero failures, 120.90 seconds. The two-minute
ULTRA simulated automation/reprepare run generated 12962 grains, peak 0.106695,
without tested callback C++ allocation, NaN/Inf or stereo divergence. Full log:
[FullTests.txt](measurements/phase11/FullTests.txt). The baseline, initial and final
CSV measurements are retained alongside it. There is no quality-acceptance PASS
hidden inside the memory-safety test result: both ULTRA candidates failed the
separate release assessment.

Remaining work before any promotion: robust epoch/residual modelling, fix octave
cancellation without introducing modulation, lower-voice and preserve/formant
coverage, detector evaluation on real noisy vocals, listening tests, fixed-delay
host integration and controlled mode-switch/FL Studio verification. These are
not implemented in Phase 11. The stable release and default were preserved.
