# Phase 3: independent formants

Vocalis 0.3.0 retains the Phase 2.5 engine and adds Formant Shift (-6 to +6 st,
0.01 st steps) and Formant Preserve (0-100%, 0.1% steps). New instances default
to Shift 0 and Preserve 100. Old states missing these controls load at Shift 0,
Preserve 0, keeping the previous uncompensated timbre.

## Architecture and exact API

`FormantProcessor` contains only control smoothing, history and mapping. It
allocates its history in preparation. `PitchShifter` still owns the spectral
engine, fixed 64-sample adapter and unity delay; there is no second audio stage.
LIVE uses two independent mono engines for stereo, as in Phase 2.5. QUALITY
retains one linked stereo engine. Each channel undergoes one spectral pass.

The unchanged Signalsmith Stretch 1.3.2 revision is
`a670068d9aeb64913331d5cc29337b19a457a7df`; its MIT license and the Signalsmith
Linear license are unchanged. No new dependency or license was introduced.

The native calls are:

- `setTransposeSemitones(P)` for pitch.
- `setFormantSemitones(S + (1 - A) * P, true)` for the envelope, where `S` is
  Formant Shift and `A` is Preserve / 100.
- When `S - A * P == 0`, `setFormantSemitones(0, false)` skips identity envelope
  correction and reproduces the Phase 2.5 spectral path.
- `setFormantBase(200 / sampleRate)` in preparation supplies a fixed envelope
  analysis reference. The pinned implementation interprets this as cycles per
  sample. Its optional fundamental estimator is therefore **not used**.

[Exact upstream implementation](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/a670068d9aeb64913331d5cc29337b19a457a7df/signalsmith-stretch.h)
implements compensation by sampling the input envelope at mapped frequencies
and adjusting spectral energies within the existing processing pass. This is
native spectral-envelope transformation, not a bank of voice EQ presets.

Pitch, Shift and Preserve ramp for 50 ms. Control histories follow the analysis
centre; native setters run once per 64 samples, with actual spectral work at the
existing hop interval. No FFT configuration occurs in the audio callback.

At Preserve 100%, the envelope target follows Shift alone. At 0%, pitch-induced
envelope movement remains and Shift is added. At 50%, half the movement remains
in semitone space. Example: Pitch +4 / Shift +1 targets +1, +3 or +5 envelope
semitones at Preserve 100%, 50% or 0%. These are approximate envelope targets,
not measured anatomical changes or a wet/dry mix of two engines.

Neutral Pitch 0 / Shift 0 returns exact delayed audio after settling, regardless
of Preserve. Gain, dry/wet and bypass keep the previous shared delay. Formant-only
processing now keeps the engine audible instead of selecting the neutral copy.

## Latency

The reporting formula remains `inputLatency() + outputLatency() + 64`.
Tests verify these values with all requested formant and combined settings.

| Rate | LIVE samples | LIVE ms | QUALITY samples | QUALITY ms |
| --- | ---: | ---: | ---: | ---: |
| 44,100 | 2,180 | 49.433 | 5,356 | 121.451 |
| 48,000 | 2,368 | 49.333 | 5,824 | 121.333 |
| 96,000 | 4,672 | 48.667 | 11,584 | 120.667 |

All are unchanged from Phase 2.5. LIVE retains its 48 ms window / 3 ms hop,
QUALITY 120 ms / 15 ms; neither uses split computation. Formants add spectral
energy calculations, not another delay stage. Around 49 ms plus device latency
is still noticeable for vocal monitoring; host compensation cannot remove that
physical delay. QUALITY remains useful for playback quality.

## Test interpretation

Tests cover both modes, mono/stereo, 44.1/48/96 kHz, Shift 0/+1/+3/+6/-1/-3/-6,
and the requested combined shifts. Buffer partitions include 1/17/64/127/256/
511/1024 samples and must produce bit-identical output to fixed 64-sample calls.
Processor alignment tests additionally exercise buffers up to 4096 samples,
dry/wet, parameter bypass and host bypass with active formants.

The voiced probe is a 120 Hz harmonic series with broad synthetic vowel resonances
near 700 and 2100 Hz. Frequency analysis maximizes summed harmonic magnitudes
on a 1-cent grid around the expected note, using a Hann-windowed steady section.
The formant-only cases measure 0 cents on this grid, **not proof of perfect pitch
preservation for every recording**. Combined shifts reach 21 cents error in LIVE
and 1 cent in QUALITY on this probe. The regression bounds are 3 cents for
formant-only and 25 cents for combined shifts.

Envelope measurements integrate spectral magnitude from 260 to 1390 Hz in
10 Hz steps. They check actual envelope movement at +/-3 and decreasing
pitch-induced movement at Preserve 0/50/100. Sampling only nominal harmonic
frequencies is unsuitable here: the shifted LIVE harmonics have small frequency
errors, which would bias the envelope measurement.

At 48 kHz LIVE, the first-band centroid for Pitch +3 / Shift 0 changes from
783.6 Hz (Preserve 0) through 716.9 Hz (50) to 650.9 Hz (100); the neutral
reference is 704.0 Hz. Preservation moves closer overall but can overcorrect.
This demonstrates the approximate nature of fixed-reference envelope recovery.
It does not establish perceptual naturalness or a literal first-formant frequency.

Identical LIVE stereo channels remain matched. Linked QUALITY has small numerical
channel differences (up to about 0.24% relative RMS in the tested extreme case),
so its regression checks bounded mismatch instead of demanding identical samples.
Both channels use the same sample clock and output length.

Allocation hooks cover tested C++ `new`, arrays and aligned allocation, including
native active formants in LIVE and QUALITY. They do not intercept every possible
OS allocation API. All DSP storage is prepared outside processing; callbacks do
not access files, log, lock, touch the UI, or reconfigure transforms.

## Limits and listening

No real vocal recordings or FL Studio listening session were available here.
Automated synthetic checks cannot certify absence of all audible clicks, metallic
colour, transient smearing, or ASIO underruns. Fixed 200 Hz envelope smoothing is
a deliberate approximation without pitch detection; very low/high voices,
breathy consonants, sparse harmonics and extreme combinations may sound less
natural. +/-6 formant shifts need not produce a proportional perceived change.
LIVE's short-window pitch/transient and independent-stereo tradeoffs remain.
The GUI's Pitch Orb still displays `--`.

Use the recorded-vocal suggestions in README with Preserve 100 initially. Compare
0/50/100, automate all three voice controls, save/reopen the project, and audition
consonants, sustained low vowels and transients. No suggested voice settings are
hard-coded presets.

## CPU and deadline measurements

Release MSVC, Intel Core i5-13600K, stereo, 3 seconds per case. Same methodology
as Phase 2.5: pre-render 16 harmonics with vibrato plus seeded noise, measure only
`PitchShifter::process()` using a steady clock, exclude synthesis/preparation,
and divide elapsed processing time by audio duration. Percentages are of one
serial audio processing budget, not total Windows CPU. Timing is offline on the
normal scheduler, without affinity or realtime priority. Other tests/builds were
not running during this recorded benchmark; scheduling outliers remain included.

Cases: neutral 0/0, legacy pitch -5.5/0/Preserve 0, pitch -5.5/0/Preserve 100,
formant 0/+3/100, combined -5.5/-2/100. Columns below use 64-sample host buffers.
The archive includes both 64 and 256 samples, p99, maximum call time and misses.

| Mode | Rate | Neutral | Legacy pitch | Pitch + preservation | Formant only | Pitch + formant |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LIVE | 44100 | 4.09% | 4.87% | 5.79% | 4.97% | 5.69% |
| LIVE | 48000 | 4.12% | 4.88% | 5.59% | 4.81% | 5.62% |
| LIVE | 96000 | 8.62% | 10.78% | 11.94% | 10.35% | 11.74% |
| QUALITY | 44100 | 1.95% | 1.99% | 2.21% | 2.05% | 2.27% |
| QUALITY | 48000 | 1.92% | 2.07% | 2.24% | 2.04% | 2.27% |
| QUALITY | 96000 | 3.71% | 4.04% | 4.54% | 4.27% | 4.72% |

At 48 kHz/64, preservation adds about 14.5% processing time versus the current
legacy-path baseline (5.59% versus 4.88% LIVE). Combined transformation costs
5.62%; the previous Phase 2.5 recorded LIVE value was 4.89%. The unchanged
QUALITY configuration uses less average CPU because its hop is much larger,
although it has longer latency and larger individual work bursts.

At 48 kHz there were no observed deadline misses in any case at either buffer
size. At 96 kHz/64, LIVE had 7/8/12/13/7 misses for neutral/legacy/pitch/formant/
combined; QUALITY had 2/7/31/17/50. QUALITY combined also had one scheduling
outlier at 44.1 kHz/64. No misses were observed in the 256-sample cases at any
rate. These are calls exceeding `samples / sampleRate`, not measured FL Studio
ASIO underruns. Extra formant work increases deadline pressure; small buffers
at 96 kHz remain unsuitable for a blanket realtime guarantee.

Reproduce from `build/`:

```powershell
& './VocalisTests_artefacts/Release/VocalisTests.exe' --formant-benchmark
```

The Phase 2.5 `--benchmark` path explicitly disables preservation so its original
measurements retain their meaning. The new benchmark keeps preservation active
for the new default pitch-only case, and includes a legacy case for comparison.

## Evidence and files

- [CPU measurements](measurements/phase3-cpu.csv)
- [Pitch and envelope measurements](measurements/phase3-quality.csv)
- [Complete test output](measurements/phase3-tests.log)
- `build/phase3-build.log`: Release build log.
- `build/Vocalis-ui.png`: rendered editor snapshot.

New files: `Source/ParameterIDs.h`, `Source/dsp/FormantProcessor.h/.cpp`,
`Tests/FormantTests.cpp`, this document and the Phase 3 measurement archives.
Changed: `Source/PluginProcessor.h/.cpp`, `Source/dsp/PitchShifter.h/.cpp`,
`Source/PluginEditor.h/.cpp`, `Source/ui/BubbleKnob.cpp`, `CMakeLists.txt`,
`Tests/ProcessorTests.cpp`, `Tests/PitchTests.cpp`, `Tests/LatencyTests.cpp`,
`Tests/LatencyBenchmark.cpp`, and `README.md`.
