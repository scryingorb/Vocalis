# Phase 12: ULTRA30 vocal validation

## Decision

ULTRA30 remains experimental and hidden. LIVE remains the production default.
The Release VST3 does not contain the experimental engine. No parameter IDs,
presets, state format, production DSP, or UI were changed. QUALITY remains an
internal option. No dependency was added. The original 0.9.0 archive is unchanged.

Phase 12 improves the existing Phase 11 TD-PSOLA implementation rather than
replacing it. Technical tests passed, but persistent octave ambiguity and limited
human listening prevent Beta acceptance. This is not a claim of validated live
vocal quality.

## Changes

- Lower ULTRA30 eligibility floor from 100 to 75 Hz; validation starts at 80 Hz.
  Source windows shrink where necessary to fit the existing causal lookahead.
  Unsupported period/formant combinations still crossfade to aligned dry audio.
- Predict continuous grain epochs, search locally, and interpolate peak position.
- Normalize accumulated overlap weights to reduce periodic gain modulation.
- Gate unvoiced material using delayed energy/difference-energy history and wait
  for actual overlap coverage before fading toward wet audio.
- Add a bounded tracking guard: two confident frames to acquire, three consistent
  frames to accept octave leaps, and release after two invalid observations.
  This guards isolated jumps; it cannot resolve a persistently wrong detector.

Buffers are allocated in preparation. No new callback allocation, locks, I/O, UI
work, or dependency was introduced. Humanize/Bloom still operate through shared
control mappings. A frozen Phase 11 engine in Tests/reference permits identical
input/settings comparisons. Production detectors and music-theory logic remain
unchanged. ULTRA20 remains research-only and was not the optimization target.

## Corpus and method

23 sources: low/mid/high synthetic registers (120/200/330 Hz), each with steady
vowel, vibrato, slide/legato, fast notes, quiet/breathy, loud, and consonant/noise
fixtures; plus two locally generated Windows SAPI spoken passages (David/Zira).
The additive fixtures are deterministic. Consonants/plosives/sibilants are proxies,
not a phonetic synthesizer. SAPI output is generated speech, **not human-recorded
singing**. OS voice versions can change its output; hashes preserve this run.
No commercial recordings or voice models are redistributed with the plugin.
Original text and voice provenance are in the measurement folder.

341 comparisons at 48 kHz / 128 samples cover Natural/Pop/Hard Tune control
settings, manual pitch +/-1/2/3/4/6, vowel formants +/-1/2/3, vibrato Humanize
0/25/50/75/100, and Bloom 0/50/100. Identical input/settings feed DRY, frozen
Phase 11 ULTRA30, revised ULTRA30, LIVE, and QUALITY. These are controlled test
settings, not an assertion that every setting equals a factory preset.

The 1364 blind WAVs contain a dry Reference and shuffled A/B/C for revised
ULTRA30/LIVE/QUALITY. Nominal delays are removed for aligned listening. Static
RMS matching uses a common target, at most 2x boost, and a 0.89 sample-peak cap;
no compressor/limiter is used. This is RMS matching, not LUFS/true-peak mastering.
These aligned files cannot measure monitoring delay. Frozen Phase 11 results
are quantitative references, not an additional blind slot.

Metrics describe the latency-trimmed source-duration window; a 300 ms rendered
tail is also checked for finite/bounded output. `supported_fraction` counts
eligibility over source plus tail, not the proportion heard fully wet. Ignore
pitch scores outside the observer's 65–1100 Hz range or without an observation.
Formant centroids are power-weighted in 250–1400 Hz, not perceptual quality scores.

## Results and limits

- At 80 Hz, Phase 11 passed +/-3 st through unchanged (300-cent errors). Revised
  ULTRA30 measured +3 at 95.1383 Hz (+0.0317 cents) and -3 at 67.2736 Hz
  (+0.0485 cents). Tests also cover 90/100/110/120/150/200/330/440 Hz.
- Main steady-vowel manual +/-1..4/6 cases were within 0.4 cents. Focused
  in-range moderate pitch cases were within 1.04 cents. This says nothing about
  perceptual graininess or naturalness of recorded singing.
- At 80 Hz, formant +/-3 retained approximately 80 Hz F0 and moved the centroid
  from the old unchanged 763 Hz to approximately 854 / 606 Hz. In-range focused
  formant tests reached 1.29 cents worst F0 error. Timbre naturalness is unverified;
  this time-domain formant method is not equivalent to Signalsmith's envelope model.
- Sustained synthetic unvoiced regions null to dry for both old and revised
  engines. Boundary maximum adjacent step improved in some cases (120 Hz +3:
  0.0803 to 0.0509), not all. This does not establish absence of audible clicks.
- An 8x second harmonic caused **263/263 steady octave errors** in both raw and
  guarded estimates at 80/100/150/220 Hz. Weaker 0.5x/2x cases had none. Confidence
  loss/reacquisition and persistent ambiguity defeat simple continuity protection.
  The isolated-jump guard passes unit tests but this major limitation remains.
- Raw corpus maxima: revised ULTRA30 0.938, Phase 11 ULTRA30 1.307, LIVE 1.011,
  QUALITY 1.026. Samples at/above unity: 0 / 313 / 3 / 7 respectively. Near-full-scale
  speech needs input headroom in production. These are floating sample peaks,
  not a new limiter or a claim of guaranteed gain bounds for arbitrary input.
- Extreme upward-octave cancellation from Phase 11 remains outside this pass's
  target. Breathy/unvoiced fallback, graininess, doubling, flutter, and formant
  instability still require broader human listening; automated metrics cannot
  declare them inaudible.

### Human observations (unblinding information)

User: "test 01 A sounds the worst" and "test 06 c sounds the worst".
Both least-preferred slots were LIVE: low-vowel Natural and low-vowel Pitch +3.
Neither answer ranks ULTRA30 against QUALITY, nor validates speech, transitions,
or singing. No subjective ratings were invented for unreviewed files.

## Timing

| Sample rate | ULTRA30 fixed scheduling | LIVE host delay | QUALITY delay |
|---|---:|---:|---:|
| 44.1 kHz | 1323 / 30 ms | 2180 / 49.4331 ms | 5356 / 121.4512 ms |
| 48 kHz | 1440 / 30 ms | 2368 / 49.3333 ms | 5824 / 121.3333 ms |
| 88.2 kHz | 2646 / 30 ms | 4297 / 48.7188 ms | 10648 / 120.7256 ms |
| 96 kHz | 2880 / 30 ms | 4672 / 48.6667 ms | 11584 / 120.6667 ms |

Neutral ULTRA delay is verified exactly. Active pitch-synchronous epochs move
relative to this schedule: at 48 kHz the active marker peak was 1419 samples and
energy centroid 1416.86. Full active timing/source-age measurements are archived.
This research scheduling figure is not an already integrated JUCE host latency.
Production dry/wet and bypass alignment and latency reporting remain unchanged.

Cold detector acquisition remains approximately 27–29 ms fast versus 37–41 ms
balanced in the tested fixtures. At 48 kHz a warm note step measured 15.31 ms
input detection, 9.98 ms corrector target decision, and 44.65 ms settled corrected
output for ULTRA30 (34.67 ms after target). Separate observers/decision thresholds
explain why target time can precede the observer's detection timestamp. Output
settling requires three observer frames within 20 cents and includes observer
delay. LIVE settled at 73.98 ms, QUALITY at 148.65 ms in this test. None of these
figures is interface/ASIO round-trip latency or a general response guarantee.

## CPU

Stereo active benchmark; microseconds per callback, mean / worst / deadline
misses. Short offline runs on this machine, not ASIO certification.

| Rate / buffer | Revised ULTRA30 | LIVE | QUALITY |
|---|---:|---:|---:|
| 48k / 32 | 8.84 / 47.9 / 0 | 55.73 / 323 / 0 | 22.62 / 509 / 0 |
| 48k / 64 | 17.82 / 132 / 0 | 112.15 / 367.7 / 0 | 46.09 / 524.8 / 0 |
| 48k / 128 | 35.59 / 79.6 / 0 | 223.56 / 388.8 / 0 | 89.32 / 529.8 / 0 |
| 48k / 256 | 71.71 / 198.2 / 0 | 441.35 / 595.3 / 0 | 177.82 / 539.5 / 0 |
| 96k / 64 | 12.98 / 65.6 / 0 | 123.03 / 1391.8 / 2 | 46.82 / 1106.7 / 60 |
| 96k / 128 | 26.06 / 78.5 / 0 | 245.11 / 666.1 / 0 | 93.97 / 1117 / 0 |
| 96k / 256 | 52.13 / 145.8 / 0 | 492.01 / 722.5 / 0 | 187.32 / 1107.7 / 0 |

Fresh pre-edit ULTRA30 at 48k/64 was 16.96 us mean / 80.1 us worst: revised mean
is about 5.1% higher. Timing outliers vary between runs. ULTRA30 had zero observed
misses over all measured rates/buffers. Cheaper CPU at higher sample rates in
this fixture does not imply universally better scaling. See raw CSVs for the
complete 32/64/128/256/512 matrix and exact callback counts.

## Verification and reproduction

Release build succeeded. Complete CTest: 100%, zero failures, 137.31 seconds.
Coverage includes 44.1/48/88.2/96 kHz, mono/stereo, variable buffers, preparation
changes, presets, state, automation, dry/wet, both bypass paths, hostile input,
NaN/Inf protection, concurrent UI/state controls and tested C++ callback allocation.
Production output remains sample-exact against the archived Phase 7 plugin.
The ULTRA two-minute simulated automation run passed. Safety PASS is separate
from quality acceptance. No live FL Studio microphone test was performed here.

From the project root, use the existing CMake Release build instructions. Run:

```powershell
tools/cmake-3.31.8-windows-x86_64/bin/ctest.exe --test-dir build -C Release --output-on-failure
build/VocalisTests_artefacts/Release/VocalisTests.exe --vocal-qa
build/VocalisTests_artefacts/Release/VocalisTests.exe --vocal-focused
```

The corpus renderer consumes local `build/phase12-corpus/spoken/Speech0.wav` and
`Speech1.wav` when available. Preserve these and their provenance/hashes to
reproduce this exact speech corpus. Additive fixtures regenerate deterministically.

Auditions: `build/phase12-qa/blind`. Keep ANSWER-KEY.csv closed while rating the
ListeningWorksheet.csv. Compare Reference then A/B/C at fixed playback volume;
record consonants, pitch stability, graininess, vibrato, transitions and timbre
before unblinding. Suggested next cases: Test0316 (speech Natural), Test0321
(speech +3), Test0203 (consonants +3), Test0043 (legato Hard Tune). Do not judge
live delay from aligned exports. Real microphone singing and challenging low,
breathy sources are still needed before promotion.

Measurements: [phase12](measurements/phase12). The Release bundle is
`build/Vocalis_artefacts/Release/VST3/Vocalis.vst3`; install the entire bundle using
the README's Windows/FL Studio instructions. There is deliberately no ULTRA
selector to test in that production bundle. Future promotion requires resolving
octave ambiguity, human acceptance, and host-safe integration; it is not part of
this completed research pass.
