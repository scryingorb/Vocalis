# Phase 6: Humanizer

Vocalis 0.6.0 adds a dedicated control-domain humanizer in
`Source/dsp/Humanizer.h` and `Source/dsp/Humanizer.cpp`. It modifies the existing
pitch corrector rather than running another audio or spectral stage. No new
library, random generator, preset system or audio latency stage was added.

## Controls

All four parameters are automatable APVTS floats, 0-100% in 0.1% steps, default 0.
Old sessions missing these IDs restore to zero, preserving the earlier tuning
behavior. Existing parameter IDs remain unchanged.

| UI label | DAW parameter | Stable ID | Behavior |
| --- | --- | --- | --- |
| Humanize | Humanize | `humanize` | Overall naturalness: retain movement, soften centering and ease transitions |
| Vibrato | Vibrato Preserve | `vibratoPreserve` | Preserve more existing vibrato by reducing instantaneous hard correction |
| Drift | Pitch Drift | `pitchDrift` | Retain a bounded amount of slow measured pitch movement |
| Smoothness | Transition Smoothness | `transitionSmoothness` | Slower correction around note changes and longer target confirmation |

Humanize is a macro, not a bypass for the other three sliders. The refinements
also work with Humanize at 0. Set **all four to 0** for the earlier tuning
behavior. Existing Natural / Hard Tune remains the starting point: additional
vibrato preservation cannot make its already fully Natural setting more natural.
Humanization only affects automatic correction. At Correction Amount 0, it does
not detune manual pitch/formant processing or otherwise generate pitch movement.

## DSP mapping

Normalized Humanize H contributes to three effective controls:

- Vibrato retention: `0.7 H + (1 - 0.7 H) V`.
- Drift retention: `0.5 H + (1 - 0.5 H) D`.
- Transition easing: `0.6 H + (1 - 0.6 H) S`.

Each new control uses a roughly 50 ms linear ramp at the detector-update rate.
The final correction still ramps sample by sample through the retune smoother.
The [JUCE smoothing utility](https://docs.juce.com/master/classjuce_1_1SmoothedValue.html)
is already part of the pinned dependency; no dependency update was needed.

Vibrato retention reduces effective Hard Tune, shifting the correction reference
from instantaneous detected F0 toward the existing slower pitch centre. It
preserves the singer's movement; it does not synthesize vibrato or claim to
classify expressive intent perfectly.

For note centering, the correction error E becomes
`E - W * tanh(E / W)`, with the W=0 case returning E unchanged. W grows from
approximately `0.04 H` to `0.08 H` semitones over a 250 ms sustain envelope.
Thus centering progressively relaxes while a note is held, retaining at most
8 cents from this stage at maximum Humanize. This changes correction strength
smoothly over time instead of adding independent pitch noise.

Drift uses two filtered versions of detected pitch: a 350 ms follower and a
1.5 second anchor following that follower. Their difference passes through a
soft bound of +/-0.04 semitones and is scaled by effective Drift. It can retain
up to 4 cents of slow movement. Perfectly constant input produces no drift;
there is no oscillator or random walk. Filters and the sustain envelope reset
on a confirmed new target or voiced reacquisition. Unvoiced input clears the
humanizer's voiced state; the existing correction release handles the fade.

Smoothness adds up to 60 ms to note-target confirmation and up to 90 ms to the
retune time constant near a new note, falling to 36 ms on a settled sustain.
The existing Retune Speed / Note Transition / Hard Tune settings still apply.
This is deliberately less abrupt near note changes; high settings can soften
intentional fast runs or slides too much.

Soft centering and drift can together retain up to 12 cents relative to the
chosen correction reference. That is **not** a bound on total output error:
preserved vibrato, Correction Amount below 100, transitions, detector error and
the spectral shifter contribute separately. The final automatic correction
continues to be clamped to +/-3 semitones and scaled by Correction Amount.

## Realtime behavior and compatibility

The Humanizer has fixed scalar state and four JUCE smoothers. It allocates no
storage and performs no locks, I/O, logging or UI calls. The detector's existing
update cadence drives it independently of host block partitioning. The existing
stereo reference, control alignment and formant compensation remain in place.
LIVE and QUALITY still use one spectral pass per channel.

Reported latency is unchanged:

| Sample rate | LIVE samples / ms | QUALITY samples / ms |
| --- | ---: | ---: |
| 44.1 kHz | 2,180 / 49.43 | 5,356 / 121.45 |
| 48 kHz | 2,368 / 49.33 | 5,824 / 121.33 |
| 96 kHz | 4,672 / 48.67 | 11,584 / 120.67 |

Host compensation, dry/wet alignment and bypass behavior are retained. This
change does not remove the existing microphone-monitoring delay or small-buffer
CPU/deadline limitations. Detector limitations also remain: use one dry vocal;
stereo detection uses the left channel as its shared reference.

## Validation

Focused tests can be run from `build/` using:

```powershell
& './VocalisTests_artefacts/Release/VocalisTests.exe' --humanizer-tests
```

Tests exercise the macro and all three refinements independently, bounds,
control smoothing, sustain-dependent strength, no generated drift on steady
input, voiced gating, state round-trip, legacy migration, automation and resets.
Rendered tests cover mono/stereo, 44.1/48/96 kHz, LIVE and QUALITY, variable host
buffers and exact equality between different partitions of the same input.
Allocation hooks cover tested C++ new/array/aligned allocations in callbacks.
They do not intercept every possible OS allocation mechanism.

At 48 kHz LIVE, a synthetic 5 Hz vibrato probe with +/-22 cents input modulation,
full correction and Hard Tune 100 produced these 5th-95th percentile spans:

| Humanizer setting | Rendered pitch span |
| --- | ---: |
| All four zero | 7.04 cents |
| Humanize 100, other three zero | 37.24 cents |
| Vibrato 100, other three zero | 40.26 cents |

On a steady input 25 cents above A3, all-zero humanization produced a mean
approximately 4.61 cents above A3; all four controls at 100 produced approximately
11.50 cents above A3. This includes LIVE's pre-existing pitch-shift error and
demonstrates softer note centering rather than perfect centering.

These are synthetic measurements, not subjective listening results. Real vocal
naturalness, consonants and fast phrases require auditioning in FL Studio.
Breathy/noisy recordings and detector errors can still cause artifacts. The
bounded drift follows measured input, so inaccurate detection can influence it.

## Manual starting settings

Select the song's actual key and scale; use a dry vocal with Input/Output 0 dB,
Wet 100%, Bypass off, manual Pitch/Formant 0 and Formant Preserve 100%.
All values below are percentages and are suggestions, not built-in presets.

| Parameter | Transparent correction | Polished pop | Obvious Auto-Tune-style effect |
| --- | ---: | ---: | ---: |
| Correction Amount | 70 | 95 | 100 |
| Retune Speed | 40 | 75 | 100 |
| Note Transition | 55 | 25 | 0 |
| Natural / Hard Tune | 0 | 55 | 100 |
| Humanize | 75 | 30 | 0 |
| Vibrato | 80 | 40 | 0 |
| Drift | 15 | 10 | 0 |
| Smoothness | 70 | 30 | 0 |

Compare a sustained vowel with vibrato, a steady note and a phrase with quick
note changes. Increase Smoothness if transitions feel abrupt; reduce it if
note changes feel sluggish. Compare all-zero humanization at the same correction
settings to isolate its effect. Automate each new control and save/reopen the
FL Studio project to verify recall in your session.

The HUMANIZE panel uses four short labels, tooltips and the existing green/aqua
style, within the unchanged 900 x 900 editor footprint.

## Build result and artifacts

Release VST3 and test executable built successfully with MSVC. The complete
CTest suite passed in 86.74 seconds, including VST3 discovery/load with all
17 parameters and all prior pitch/formant/correction regressions. The final
build log contains no compiler warnings or errors.

Evidence: [rendered measurements](measurements/phase6-humanizer.csv),
[complete test output](measurements/phase6-tests.log),
`build/phase6-build.log` and `build/Vocalis-ui.png`.

New files: `Source/dsp/Humanizer.h/.cpp`, `Tests/HumanizerTests.cpp`, this guide
and the Phase 6 measurement archives. Integration updates: `PitchCorrector`,
`PitchShifter`, processor/APVTS, centralized parameter IDs, editor, CMake,
processor test entry point and README. Dependency versions/licenses are unchanged.

Updated Release bundle:

```text
C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3
```

Close FL Studio before replacing the installed bundle. Copy the entire bundle,
including Resources, to `C:\Program Files\Common Files\VST3\Vocalis.vst3` and
rescan the updated plugin as described in README. No system installation was
performed as part of this build.
