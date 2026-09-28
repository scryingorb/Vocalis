# Phase 7: Bloom and factory presets

Vocalis 0.7.0 adds an automatable Bloom macro and nine factory sounds. The new
`Source/ui/BloomControl.h/.cpp` uses the existing glossy green knob style.
`Source/presets/PresetManager.h/.cpp` owns the factory parameter table and recall.
No dependency, separate effect engine or additional audio delay was introduced.

## Bloom

Bloom (`bloom`) ranges from 0 to 100% in 0.1% steps and defaults to zero. Older
states missing Bloom restore to zero. The mapping is centralized in
`Source/dsp/BloomMapping.h` and can be edited without changing the UI or presets.

For normalized Bloom B (0-1), the effective DSP controls are:

| Control | Mapping |
| --- | --- |
| Humanize | base + (100 - base) * 0.60 * B |
| Vibrato Preserve | base + (100 - base) * 0.50 * B |
| Transition Smoothness | base + (100 - base) * 0.55 * B |
| Formant Shift | clamp(base + 0.75 * B, -6, +6) semitones |

Lower Bloom returns toward the user's base settings; it does not override a
manually chosen high Humanize value. Higher Bloom adds expression and softer
transitions with a subtle brighter formant character. These are artistic voicing
choices, not a guarantee of naturalness on every singer.

Bloom does **not** adjust Input Gain, Output Gain, Dry/Wet, manual Pitch, key,
scale, Correction Amount, Formant Preserve or Pitch Drift. It never writes other
APVTS parameters while processing. Their knobs continue to show the independently
editable **base values**; Bloom adds the documented effective offsets. This
avoids automation feedback and lets manual edits continue working after recall.
The existing formant/humanizer ramps smooth effective changes over approximately
50 ms. The final correction continues using its existing sample-wise smoothing.

At Correction Amount 0, only Bloom's formant character is audible; the Humanizer
still requires automatic correction. At Bloom 0 its mapping is identity, retaining
previous behavior. Formant saturation at +6 st is intentional.

## Factory sounds

| Preset | Pitch st | Base Formant st | Bloom % | Output dB | Intent |
| --- | ---: | ---: | ---: | ---: | --- |
| Natural | 0 | 0 | 15 | 0 | Gentle correction and preserved movement |
| Pop | 0 | 0 | 25 | 0 | Firmer correction with expressive smoothing |
| Hard Tune | 0 | 0 | 0 | 0 | Fast, strong correction; humanization off |
| Feminine | +3 | +1.75 | 30 | -1 | Moderate upward pitch and formant character, smooth tuning |
| Deep | -3 | -1.75 | 10 | -1 | Lower character with gentler shifting and smoothing |
| Airy | 0 | +0.75 | 75 | -1 | Brighter formants and soft, expressive correction |
| Robot | 0 | -0.75 | 0 | -1 | Fast, strong correction and minimal expressive movement |
| Cartoon | +7 | +3 | 15 | -3 | Deliberately exaggerated high character |
| Monster | -9 | -4 | 0 | -3 | Deliberately exaggerated low character |

All presets use Input 0 dB, Dry/Wet 100%, Bypass off and Formant Preserve 100%.
Their full normal-parameter settings live in the factory table. Feminine also
uses Humanize 50%, Vibrato 55%, Drift 10% and Smoothness 50%; its effective formant
shift with Bloom is +1.975 st, not an octave-up pitch shortcut. These labels
represent artistic characters, not universal biological voice mappings. Airy
uses existing formant/humanization processing, not synthesized breath noise.
Robot is a tuning character, not a vocoder or fixed-note resynthesizer.

**Key and Scale are retained on preset recall.** Set the song's musical context
once and audition the sounds without losing it. New instances still start in
Chromatic with correction disabled until a preset or manual setting enables it.
Factory recall sets the other 16 parameters with standard host notifications
and gestures on the UI thread. It does not replace audio buffers, rebuild DSP,
reset the engine, or introduce per-preset processing paths.

The two capsule rows follow the requested order. Matching factory settings glow.
Changing any sound parameter makes the display Custom and removes the glow;
returning all sound parameters to a factory match restores it. Changing Key or
Scale does not mark the sound Custom because these are retained musical context.
Matching is derived from actual parameters, so project recall, undo/automation
and manual editing cannot leave a stale preset-name badge. Host automation can
subsequently override recalled values normally.

These first factory presets are selected through Vocalis' capsules, not VST3
program slots or user preset files. Sessions save the ordinary APVTS parameters;
no separate preset metadata or hidden DSP state is necessary.

## Testing and realtime behavior

Every preset is rendered at 44.1/48/96 kHz, mono and stereo, with two synthetic
harmonic vowel probes (120 and 220 Hz fundamentals). Tests check finite bounded
output, gain and peaks, identical-channel synchronization, unchanged latency,
state restoration, Custom/factory matching, key/scale retention and callback
allocation safety. The 48 kHz cases also compare variable host partitions
against fixed 64-sample processing for identical output. Switching all nine
presets while automating Bloom is tested at all rates/channel layouts.

Focused tests from `build/`:

```powershell
& './VocalisTests_artefacts/Release/VocalisTests.exe' --preset-tests
```

The full Release CTest suite retains earlier gain, pitch, formant, automatic
correction, humanization, dry/wet, bypass, reset, state and VST3 loading checks.
Allocation hooks intercept tested C++ new/array/aligned allocations; they do not
cover every possible operating-system allocation mechanism. Preset application
itself is a UI action, not an audio callback operation.

Bloom's mapping uses bounded arithmetic and the existing control smoothers.
There is no file access, logging, network access, UI access, blocking lock or
DSP reconfiguration in the audio callback. LIVE latency remains 2180/2368/4672
samples at 44.1/48/96 kHz (49.43/49.33/48.67 ms). The existing device-latency and
small-buffer deadline limitations still apply.

The presets use modest output trims for transformed characters and no positive
gain boosts. This is not a limiter or universal loudness normalization: vocal
spectra differ and input can already be clipping. Synthetic tests cannot prove
subjective vocal quality or absence of artifacts on every voice. Real-vocal
listening in FL Studio remains a manual step.

## Audition in FL Studio

Replace the entire installed Vocalis VST3 bundle while FL Studio is closed and
rescan as described in README. Route a dry single vocal to Vocalis, set the
song's key/scale, and click each capsule. Compare Deep against Monster and
Feminine against Cartoon. Sweep Bloom from 0 to 100 without changing Dry/Wet;
listen to vowel character, vibrato and note transitions. Manually edit Pitch or
Humanize, confirm Custom, then reload the capsule. Save/reopen the project to
check both the sound and matching glow. Input pitch is shown beneath Bloom.

Use a mono vocal or stereo with the lead on the left, following the existing
pitch detector's reference-channel behavior. Avoid judging pitch correction on
polyphonic material or a reverberant full mix.

## Final Release results

The Release VST3 and tests built successfully without compiler warnings/errors
in the final build log. The complete CTest suite passed in **108.92 seconds**.
The generated VST3 was discovered and loaded with all 18 parameters by the JUCE
test host. Capsule click tests exercised every preset and verified that exactly
one matching capsule glows. The rendered snapshot shows Feminine selected.

Across 108 preset/rate/channel/voiced-probe combinations, steady-section RMS
change ranged from **-6.96 dB to approximately 0 dB**. Maximum measured output
peak was **0.4254** (approximately -7.43 dBFS). No tested preset clipped or
produced an extreme gain boost. These presets are not loudness-matched: the
stylized, conservatively trimmed sounds can be quieter. Values describe the
two test probes, not all possible recordings.

All preset switching, state restoration, manual editing, key/scale retention,
buffer partition, latency, stereo synchronization and tested callback allocation
checks passed. Earlier DSP regressions also passed. No interactive FL Studio
session or real-vocal listening was performed.

Evidence: [preset levels](measurements/phase7-presets.csv),
[complete test output](measurements/phase7-tests.log), `build/phase7-build.log`,
and `build/Vocalis-ui.png`.

New files include `Source/ui/BloomControl.h/.cpp`,
`Source/presets/PresetManager.h/.cpp`, `Source/dsp/BloomMapping.h`,
`Tests/PresetTests.cpp`, this guide and measurement archives. Integration changes
are in the processor/APVTS, centralized IDs, editor, CMake, test entry point and
README. Dependency versions and third-party notices are unchanged.

Updated Release bundle:

```text
C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3
```

Copy the entire outer bundle to `C:\Program Files\Common Files\VST3` when
installing. The bundle has been built, but no system installation was performed.
