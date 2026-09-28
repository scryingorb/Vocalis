# Vocalis

Windows 10/11 x64 VST3 vocal processor using C++17, JUCE 8.0.12, CMake and MSVC.
Manual pitch shifting (-12 to +12 st) and independent Formant Shift (-6 to +6 st),
both in 0.01 st steps, with Formant Preserve (0-100%, default 100%).
Automatic monophonic pitch correction now adds Key, Scale, Correction Amount,
Retune Speed, Note Transition and Natural / Hard Tune. Correction defaults to 0%
to preserve existing sessions. The central Pitch Orb displays detected input note and cents when correction is enabled.
Phase 6 adds Humanize, Vibrato Preserve, Pitch Drift and Transition Smoothness.
They influence existing correction, with bounded input-derived movement and no
random pitch noise. All four default to zero for compatibility. Phase 7 adds the Bloom macro and nine factory preset capsules.
Phase 8 adds a resizable mint/aqua glass interface, reusable bubble controls, a central Pitch Orb and input/output meters. See [UI design and validation](docs/UIRedesign.md).
See [Bloom mapping and factory presets](docs/BloomPresets.md).
See [Humanizer behavior, tests and listening settings](docs/Humanization.md).
See [Phase 4 controls, architecture and validation](docs/PitchCorrection.md).
See [Phase 3 DSP and measurements](docs/FormantProcessing.md).

Phase 2.5 reduces default latency from about 121 ms to **49 ms** with an internal
LIVE configuration. Original QUALITY settings remain in the preparation API,
without a UI selector or automatable mode parameter. See the
[latency investigation](docs/LatencyOptimization.md) for measurements and tradeoffs.

## Technology and build

Requires Visual Studio 2019 (16.11) or 2022 with Desktop development with C++, a
Windows SDK, CMake 3.24+, and Git. This workspace includes portable CMake 3.31.8
under `tools/` and dependency sources under `external/`.

From PowerShell in this directory:

```powershell
$cmake = "$PWD/tools/cmake-3.31.8-windows-x86_64/bin/cmake.exe"
& $cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DVOCALIS_BUILD_TESTS=ON
& $cmake --build build --config Release --target Vocalis_VST3 VocalisTests --parallel 2
& "$PWD/tools/cmake-3.31.8-windows-x86_64/bin/ctest.exe" --test-dir build -C Release --output-on-failure
```

For a fresh checkout, use `cmake` and `ctest` from PATH. CMake fetches pinned
commits when the corresponding `external/` directories are absent; the first
configure then requires network access. For VS 2022 use generator
`Visual Studio 17 2022` and a separate build directory. Tests are optional and off
by default. Build Release for realtime use. Only Windows x64 VST3 is targeted.

Release bundle:

```text
C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3
```

The outer `.vst3` is a directory bundle. Its binary is
`Contents\x86_64-win\Vocalis.vst3`. Copy the entire outer bundle, including
Resources and license files, when installing.

## Install and scan in FL Studio

1. Close FL Studio before replacing the previously installed bundle.
2. Copy the new outer `Vocalis.vst3` directory to
   `C:\Program Files\Common Files\VST3\Vocalis.vst3`.
3. Open FL Studio x64, **Options > File settings > Manage plugins**, enable **Verify plugins** and
   **Rescan previously verified plugins**, then click **Find installed plugins**.
4. Add Vocalis in a Mixer effect slot via **More plugins**. Confirm the interface
   has the central **Pitch Orb**, **TUNE / VOICE / HUMANIZE** panels, glossy knobs, glowing preset capsules and bottom gain/mix/bypass controls.

See Image-Line's [plugin installation documentation](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).
Plugin identity and existing parameter IDs are preserved. Phase 1 state loads
with Pitch at 0 st, even when restored into an instance previously shifted.

## Bloom and factory presets

Click Natural, Pop, Hard Tune, Feminine, Deep, Airy, Robot, Cartoon or Monster.
Preset recall retains Key/Scale and uses normal editable plugin parameters.
A matching capsule glows; manual sound edits display Custom. Project state
restores the sound and automatically recovers the matching glow.

Bloom adds effective Humanize, Vibrato, Smoothness and up to +0.75 st of formant
character. The other knobs show their base values and remain editable. Bloom
never changes gain or Dry/Wet. Set Bloom to zero for the earlier-phase checks
below. See the linked Phase 7 guide for exact mapping, preset values and tests.

## Test humanization

The HUMANIZE panel adds an overall Humanize macro plus independent Vibrato,
Drift and Smoothness refinements. Raising Humanize preserves more movement,
softens note centres and eases transitions. Drift retains at most four cents
of slow measured input movement; it adds no random detuning. Set all four to
zero for the previous correction behavior. Correction Amount must be above zero.

The [Phase 6 guide](docs/Humanization.md#manual-starting-settings) includes
transparent, polished-pop and obvious hard-tuning starting settings, plus the
precise parameter interactions and known limitations.

## Test automatic correction

Choose the vocal's key and scale. Start with Amount 70%, Speed 40%, Transition
60%, Natural / Hard Tune 0%, manual Pitch/Formant 0 and Preserve 100%. For a
hard effect try Amount 100%, Speed 100%, Transition 0%, Hard 100%. Amount 0
turns correction off. Minor is natural minor, and reference tuning is A4=440 Hz.
Natural mode retains more vibrato; Hard follows the instantaneous detected pitch.
Manual Pitch transposes the corrected result. Stereo detection uses the left
vocal channel; route right-only material to mono first. See the Phase 4 guide
for the precise control mapping, measured limits and tests.

## Test manual pitch/formant processing in FL Studio

Set Correction Amount to 0% for the following Phase 2/3 checks.

1. Route a recorded, reasonably dry vocal to a Mixer track and insert Vocalis.
   Start with Input and Output at 0 dB, Dry/Wet 100%, Bypass off.
2. Play the vocal and set Pitch to **+3.0 st**, then **-5.5 st**. The voice should
   rise/fall while retaining duration and musical timing. Type **+2.25** into the
   value box to verify fractional control. Try +/-12 st too; extreme shifts
   naturally sound more synthetic. With Formant Preserve at 100%, pitch changes attempt to retain the vocal
   envelope. Lower Preserve to hear more pitch-linked timbre movement.
3. Double-click Pitch and Formant to return both to **0.00 st**. After settling, the original samples
   pass through with the reported delay. Leave automatic plugin delay compensation
   enabled. A null test needs a matching delay on the reference or correctly
   compensated routing. Allow about 0.2 seconds to settle after a pitch change.
4. Set Pitch +7 st and compare Dry/Wet at 100%, 50%, and 0%. At 50% you hear the
   original and shifted voices together; at 0% only the delayed original remains.
   Output Gain follows the blend. Input Gain affects only the shifted branch.
5. Engage the plugin's **Bypass** button. Pitch, Formant and both gains are bypassed via
   a 20 ms crossfade. Latency stays fixed, avoiding a timing jump. The separate
   FL Studio effect-slot enable switch is controlled by the host.
6. Move Pitch, then use **Tools > Last tweaked > Create automation clip**, or expose
   Pitch through the wrapper parameter browser. Test sweeps and steps through
   zero. Changes glide over 50 ms, with spectral processing and transitions
   adding settling time. Listen for clicks, dropouts or unexpected timing changes.
7. Repeat at **44.1 kHz**, **48 kHz** and **96 kHz**, with ASIO buffers such as **64, 128, 256,
   512 and 1024 samples**. Restart playback after changing audio settings.
8. Set Pitch -3.5 st, save an `.flp`, close and reopen it, and confirm the value and
   sound restore. Disable automation temporarily when checking static recall.

The LIVE setting adds about **49 ms** of fixed latency. DAW compensation
aligns playback tracks but cannot remove live microphone monitoring delay. Use
recorded vocals for the initial quality test. Spectral pitch shifting can produce
transient smearing or modulation, especially at extremes; automated signal tests
are not a substitute for listening to your own vocals. This is substantially
faster, but 49 ms plus audio-interface latency remains noticeable for monitoring.
At 96 kHz, start with 256-sample buffers: offline timing runs still showed some
64-sample callback deadline overruns in both the old and new configurations.

## Signal path and parameters

```text
input -> Input Gain -> PitchShifter ---------------------- wet
input -> matching fixed delay --------------------------- dry
dry/wet linear blend -> Output Gain -> bypass crossfade -> output
bypass crossfade's clean source = delayed original input
```

| Parameter | Range | Default |
| --- | --- | --- |
| Input Gain | -24 to +24 dB | 0 dB |
| Dry/Wet | 0 to 100% | 100% |
| Output Gain | -24 to +24 dB | 0 dB |
| Bypass | Off / On | Off |
| Pitch | -12 to +12 st, 0.01 st steps | 0 st |
| Formant Shift | -6 to +6 st, 0.01 st steps | 0 st |
| Formant Preserve | 0-100%, 0.1% steps | 100% |
| Key | C through B | C |
| Scale | Major / natural Minor / Chromatic | Chromatic |
| Correction Amount | 0-100% | 0% (off) |
| Retune Speed | 0-100%, higher is faster | 50% |
| Note Transition | 0-100%, higher holds targets longer | 50% |
| Natural / Hard Tune | 0% natural, 100% hard | 0% |
| Humanize | 0-100% | 0% |
| Vibrato Preserve | 0-100% | 0% |
| Pitch Drift | 0-100% | 0% |
| Transition Smoothness | 0-100% | 0% |
| Bloom | 0-100% | 0% |

At Pitch 0, Formant 0 and unity gains, all Dry/Wet settings are transparent after latency.
At Pitch 0, Input +6 dB and Output 0 dB, 100% wet raises level 6 dB; 50% raises it
about 3.51 dB; 0% retains original level. Output -6 dB cancels Input +6 dB at 100%
wet. No limiter or clipping is added.

## DSP, latency and realtime behavior

`Source/dsp/PitchShifter.h/.cpp` owns Signalsmith Stretch behind a streaming
interface. LIVE uses manual configuration: 48 ms window, 3 ms hop (approximately
16x overlap), split computation off. QUALITY retains Phase 2's 120 ms window,
15 ms hop and linked stereo. LIVE uses independent left/right spectral maps on
an identical clock to avoid short-window cross-channel note interactions.
Identical channels remain identical, but shifted stereo imagery can change.
The ratio
is `2^(semitones/12)`, with equal input/output lengths so duration stays unchanged.
Formants use the same spectral pass; no additional transform stage is added.
`FormantProcessor` smooths Shift and Preserve, delays controls to the analysis
centre, and maps them to Signalsmith native formant compensation.

The adapter processes fixed 64-sample internal frames using preallocated buffers.
Host buffers can be small, odd-sized, change size, or exceed the preparation size
hint without changing allocation or output timing. Pitch and formant controls ramp over
50 ms; control history is aligned to the engine's analysis latency. Engine pitch
updates occur every 64 samples; spectral updates follow the hop. Gain, mix,
bypass and unity-path crossfades use 20 ms ramps.

At exactly zero Pitch and Formant, the adapter returns a sample-exact delayed copy. The engine stays
running to avoid cold-start glitches. On return to zero it waits for control and
spectral history to settle, then crossfades to the clean copy.

Latency is `inputLatency + outputLatency + 64`, constant across pitch/mix/bypass:

| Sample rate | Reported samples | Approximate time |
| --- | --- | --- |
| 44,100 Hz | 2,180 | 49.43 ms |
| 48,000 Hz | 2,368 | 49.33 ms |
| 88,200 Hz | 4,297 | 48.72 ms |
| 96,000 Hz | 4,672 | 48.67 ms |

Dry, wet-unity and bypass paths share this delay. The processor reports a
conservative tail of latency plus one analysis window, about 97 ms. Host `reset()`
clears histories without allocating, preventing stale audio after a reset.
Dynamic storage and FFT planning happen during preparation. Processing uses
cached atomic parameter reads, bounded scratch storage and arithmetic; no file
or network operations, logging, blocking locks or direct UI updates.

Formants are broad vocal resonances: they affect vowel colour and perceived size.
Pitch controls musical note height; Formant moves those resonances without
intentionally changing that note. Preserve cancels a percentage of the formant
movement that pitch shifting would otherwise introduce. At 100%, the envelope
target follows Formant alone; at 0%, it follows Pitch + Formant. At 50%, half
the pitch-induced movement remains, measured in semitones. This is a target,
not a promise of perfect envelope reconstruction.

Old states missing the new parameters restore Formant 0 / Preserve 0 to retain
Phase 2 timbre. Fresh instances default to Preserve 100. All IDs remain stable.

Manual starting points with a dry recorded vocal, unity gains, 100% wet,
Bypass off and Preserve 100%:

| Character | Pitch | Formant |
| --- | --- | --- |
| Brighter / smaller | 0 | +1 to +2 |
| Darker / larger | 0 | -1 to -2 |
| Higher artistic voice | +2 to +4 | +1.5 to +3 |
| Lower artistic voice | -2 to -4 | -1.5 to -3 |

These are listening suggestions, not presets or universal biological mappings.
Automate both controls, compare Preserve 0/50/100, and save/reopen the FL Studio
project to check recall. Extremes may sound metallic or synthetic; recorded vocal
listening and actual ASIO deadline testing remain necessary.

## Files and dependencies

- `Source/PluginProcessor.*`: existing APVTS/host interface, with Pitch, latency
  compensation, old-state migration and dry/wet integration.
- `Source/dsp/BloomMapping.h`: centralized macro mapping.
- `Source/presets/PresetManager.*`, `Tests/PresetTests.cpp`: factory sounds, recall and validation.
- `Source/ui/BloomControl.*`: signature glossy macro control.
- `Source/dsp/Humanizer.*`, `Tests/HumanizerTests.cpp`: bounded control-domain humanization and regression tests.
- `Source/dsp/PitchDetector.*`, `Source/dsp/PitchCorrector.*`: causal monophonic F0 and musical correction.
- `Tests/CorrectionTests.cpp`: detector, scales, correction, automation, state and CPU checks.
- `Source/ParameterIDs.h`: centralized stable automation IDs.
- `Source/dsp/FormantProcessor.*`: smoothed, time-aligned envelope controls.
- `Tests/FormantTests.cpp`: spectral envelope, pitch preservation, automation and CPU checks.
- `Source/dsp/PitchShifter.*`: library adapter, smoothing, buffering, unity
  crossfade, reset and latency handling.
- `Source/PluginEditor.*`, `Source/ui/BubbleKnob.*`: resizable 1120 x 900 default editor, three main panels,
  glossy Pitch control and signed fractional semitone text.
- `Source/ui/VocalisLookAndFeel.*`: green/aqua styling. The editor updates its
  input-pitch readout and matching preset glow on a message-thread timer.
- `Tests/ProcessorTests.cpp`, `Tests/PitchTests.cpp`, `Tests/LatencyTests.cpp`:
  processor, pitch, timing/phase, allocation and VST3 checks.
- `Tests/LatencyBenchmark.cpp`: offline configuration, CPU, tone and transient
  comparisons; synthetic audition WAV generation.
- `CMakeLists.txt`: pinned dependencies, VST3 build, tests and bundled DSP licenses.

Signalsmith Stretch provides a dedicated spectral pitch shifter suited to voice
processing, avoiding a speed-changing resampler. It and Signalsmith Linear, its
FFT/STFT dependency, are **MIT licensed**, not GPL-only. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for pinned revisions, CMake
integration and license locations. JUCE's existing licensing remains separate.

## Validation

Release is built using MSVC 19.29, Windows SDK 10.0.19041 and CMake 3.31.8.
Automated checks cover:

- Mono/stereo at 44.1/48/88.2/96 kHz; existing gain/mix/bypass behavior.
- 0, +3, -5.5, -12, -3.5, +2.25, +7 and +12 st sine shifts. For 220/330 Hz
  regression tones LIVE tolerance is 10 cents. Extra 80/110 Hz tests permit
  35 cents absolute error and less than 1 cent steady-note drift: an explicit
  short-window compromise, not precise automatic tuning.
- Variable buffers from 1 to 4096 samples, including odd sizes, and bit-identical
  results for different host buffer partitions at a fixed parameter setting.
- Exact delayed zero-pitch impulses, clean bypass, return to zero, bounded
  discontinuity during automated sine shifts, and no C++ allocations in tested
  callbacks/reset. The hook covers new/arrays/aligned allocations, not every
  possible OS-level allocation mechanism.
- All eighteen parameters' state round-trip, Phase 1 migration, malformed state,
  editor rendering and fractional Pitch text formatting.
- Discovery, loading, reported latency and actual +7 st processing through the
  generated VST3 bundle in a JUCE test host.
- Broadband dry/wet null identities, both bypass paths, coherent stereo phase
  stability over time, identical-channel synchronization, retained QUALITY timing.

Phase 3 also checks actual spectral-envelope movement, preservation mapping,
formant-only fundamental pitch, combined shifts, both processing modes and
allocation-free rapid formant automation. See the linked Phase 3 report for CPU
and deadline results, quality limits and archived test evidence.

Phase 4 adds synthetic pitch-detection, scale, correction, vibrato, state and
allocation tests plus complete-processor CPU measurements. See the
[Phase 4 report](docs/PitchCorrection.md) for results and limitations.

Phase 6 adds bounded drift, independent refinements, sustain behavior, state,
rendered vibrato/centering and realtime humanizer automation checks.

Phase 7 tests all nine factory sounds, preset matching/recall, manual editing,
Bloom bounds, switching, levels, state and callback allocation safety.

Tests write `build/Vocalis-ui.png`. Interactive FL Studio scanning, listening on
real vocals, automation and `.flp` recall have not been performed here; use the
manual checks above.

## Prototype release (Phase 9)

Version 0.9.0, Windows x64 only. See [stability audit and release evidence](docs/ReleasePreparation.md).

### Features

- Fractional manual pitch and independent formant shaping/preservation.
- Monophonic key/scale correction, natural through hard tuning.
- Humanize, vibrato preservation, measured drift and transition smoothing.
- Bloom expression macro, nine editable factory presets and full DAW automation/state.
- Resizable Frutiger Aero interface, input-note/cents orb and peak meters.

### Screenshots

![Vocalis prototype interface](docs/measurements/Phase8UI.png)

Placeholder for release screenshots captured in FL Studio; the image above is a
JUCE-rendered UI test preview, not a DAW capture.

### Distributing and installing on another computer

After a Release build, run `powershell -NoProfile -ExecutionPolicy Bypass -File tools/PackageRelease.ps1` from the
project root. The default archive is `dist/Vocalis-0.9.0-Windows-x64.zip`. The
script refuses an existing staging directory; pass a new `-Destination` for a
subsequent package. It includes the full VST3 bundle, documentation, all notices
and a SHA-256 file manifest. Keep all of these when sharing the prototype.
PDB files, tests, source, Visual Studio, JUCE SDK and Signalsmith source are not
runtime requirements.

On the destination Windows 10/11 x64 PC:

1. Install the **Microsoft Visual C++ v14 x64 Redistributable** from
   [Microsoft's official downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
   The binary imports MSVCP140.dll, VCRUNTIME140.dll and VCRUNTIME140_1.dll plus
   Windows system libraries. JUCE/Signalsmith are compiled in; no separate DSP DLL,
   asset pack or network service is required. The runtime installer is not bundled.
2. Extract the archive and copy the entire outer `Vocalis.vst3` directory to
   `C:\Program Files\Common Files\VST3\Vocalis.vst3` (administrator permission may be needed).
3. Start FL Studio x64 and scan with Plugin Manager as described above. Keep delay
   compensation enabled. Add Vocalis to an effect slot, then check audio, automation,
   presets and saved-project recall. Close the DAW before replacing an older copy.

### Known limitations

- Approximately 49 ms plugin delay plus interface/host delay: still noticeable
  for live monitoring. No live/quality mode selector yet.
- Monophonic detector, approximately 65-1100 Hz, A4=440 Hz, natural Minor only.
  Stereo tuning follows the left input; not intended for chords or full mixes.
- Short-window pitch/formant processing can smear transients, sound metallic at
  extremes, or change stereo imagery. Humanization cannot repair detection errors.
- No output limiter: aggressive gain settings can clip downstream equipment.
- Presets preserve Key/Scale; no user-preset browser/export or MIDI note targeting.
- Pitch Orb is inactive when correction is disabled; it shows input, not output.
- Automation is block/control-quantum based rather than sample-accurate parameter
  events. Preset values update individually, with existing smoothing.
- Unsigned prototype; no installer, clean-machine certification, automated FL
  Studio UI/ASIO testing or new real-vocal listening pass. Windows x64 VST3 only.

### Roadmap

Priorities after this prototype: real-vocal listening and FL Studio release QA,
monitoring-latency/quality evaluation, broader host testing, accessibility and
packaging/signing. Future scale options, user presets and processing-mode controls
are possibilities, not features implemented in this release.

### Licence information

Retain [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and all files in LICENSES,
including the copies inside the VST3 bundle. Notices cover Signalsmith MIT, the
bundled MIT VST3 SDK, JUCE, and JUCE's codec/font/image dependencies. This product
includes software developed by the Independent JPEG Group.

JUCE is available under its commercial licence or AGPLv3; the permissive
Signalsmith licences do not remove JUCE obligations. Confirm the applicable
[JUCE 8 licence](https://juce.com/legal/juce-8-licence/) before proprietary/public
distribution. No JUCE entitlement was verified here, and no distribution licence
has yet been selected for the original Vocalis code/assets. The owner must make
that decision; a binary plus notices alone is not an AGPL source release.

## Phase 11 low-latency research

An isolated TD-PSOLA-style ULTRA LIVE candidate was evaluated at 20 and 30 ms
scheduling delays. It is **not ready for release**: low-register coverage and an
upward-octave level-collapse test failed. No ULTRA selector or mode parameter was
added. The production VST3 still defaults to the known-good approximately 49 ms
LIVE engine; QUALITY remains an internal preparation option. Existing projects
and creative presets retain their behavior.

See [LowLatency.md](docs/LowLatency.md) for measured audio/control timing, CPU,
pitch/formant tradeoffs, tests, deterministic audition files, and honest limits
on what can currently be tested in FL Studio. The new engine is test-only and
adds no external dependency. The 0.9.0 distribution archive remains unchanged.

## Phase 12 vocal validation

The isolated ULTRA30 experiment now covers tested 80–90 Hz sources and includes
epoch tracking, overlap normalization, and bounded voiced/octave guards. Release
tests pass, but sustained octave ambiguity and incomplete human listening keep
ULTRA hidden. LIVE remains the production default; existing projects, parameters,
presets and production audio are unchanged.

See [UltraLiveValidation.md](docs/UltraLiveValidation.md) for the 23-source corpus,
341 blind comparisons, measured CPU/timing, user listening observations, remaining
limitations and reproduction instructions. Generated speech is not a substitute
for recorded singing. No new external dependency was added.
