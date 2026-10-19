# VOCALIS audit — 2026-10-18

## Follow-up implementation (current source)

The sections below this follow-up retain the first audit's historical results.
The current source additionally implements the four requested improvements:

- **CPU peaks:** LIVE at 88.2/96 kHz now uses Signalsmith's existing split
  computation. At lower rates the scheduling and latency are unchanged. This
  spreads spectral work across a hop, at the explicit cost of one additional
  3 ms hop. Reported latency, dry/wet and bypass alignment include that hop.
  LIVE is now 4561 samples / 51.7120 ms at 88.2 kHz and 4960 / 51.6667 ms at
  96 kHz. This is scheduling optimization, not a reduction in total CPU work.
- **Pitch tracking:** a conservative full-period check resolves low/mid vocal
  octave errors when a strong second harmonic falsely passes the first YIN
  valley. It requires substantially better periodicity and accounts for
  fractional-lag interpolation error. High-frequency ambiguity retains the
  original choice to avoid halving valid high notes. New tests cover 48
  rate/frequency/harmonic combinations, noisy voiced segments, unvoiced noise
  and genuine octave transitions. Real breathy-vocal listening remains pending.
- **Installer:** ZIP and installer share current CTest validation and hashes of
  the entire bundle, including resources. The installer derives its version
  from CMake, accepts build/output paths, and refuses an existing output EXE.
  Failure-gate and modified/added-resource rejection tests pass. No existing
  distribution artifact is overwritten and no installer is executed.
- **UI:** a 900x700 compact layout preserves editable value fields, and all 27
  interactive controls have an explicit musical Tab order. Geometry/value-field
  tests pass at six sizes. Default 1120x900 appearance is unchanged. A generated
  JUCE snapshot was inspected; no computer-use or desktop automation was used.

A fresh matched offline before/after processor study at 96 kHz, stereo/64 gave:

| Setting | Before p99 µs | After p99 µs | Before/after mean CPU % |
|---|---:|---:|---:|
| Unity | 286.9 | 97.6 | 8.57 / 8.68 |
| Natural | 402.2 | 192.8 | 12.44 / 13.19 |
| Hard Tune | 400.5 | 130.9 | 12.45 / 12.49 |
| Automation | 393.2 | 156.2 | 12.31 / 12.58 |

These single-run measurements show lower typical callback peaks, with a small
average-work increase. Scheduler outliers remain (Natural had one deadline miss
in each run); they do not prove dropout-free ASIO operation. The first audit's
CPU numbers were collected in a separate session and are not the comparison
baseline. The full regression passed after these DSP/UI changes (135.48 s).
An additional 354-case pure/harmonic detector sweep passed with worst error
4.58159 cents. The final installer pipeline reran the full regression successfully
(137.96 s), compiled the installer, verified its 1.0.0 product version and bundle
integrity, and wrote a matching SHA-256 checksum. No named-DAW testing or human listening is claimed; the user explicitly
excluded computer use.

Evidence for the follow-up is in `docs/measurements/audit-followup/`: matched
before/after CPU CSVs, split-scheduling comparison, detector sweep and focused
correction/UI logs. The standalone sweep used 75–1090 Hz in 17.3 Hz steps,
44.1/48/96 kHz, fundamental-plus-second and missing-fundamental second/third
harmonic signals, with 0.4 s acquisition for each of 354 cases.

Final regression and installer evidence: [CTest log](measurements/audit-followup/CTest.txt)
and [installer validation log](measurements/audit-followup/InstallerValidation.txt).
The review installer is `build/audit/installer-final/Vocalis-1.0.0-Setup.exe`;
it was compiled but not installed. Its SHA-256 is
`7D179E9A07CA88468D98A50355F0E16DE428927652BFDCE06E4525B38B85CBD5`.
The tested VST3 module SHA-256 is
`32FCC187C2F316B4139E50F350CB5CE45A9E6703883C0B3C139BC22CAF1B85AF`.
The installer contains the audit snapshot from before these final acceptance
notes were added. Existing `dist` artifacts remain unchanged.

## Scope and initial architecture

The initial worktree was clean. Existing remote:
`https://github.com/scryingorb/Vocalis.git`. No clone, dependency upgrade,
parameter-ID change, commit, push or publication is part of this audit.

- `PluginProcessor` owns 18 APVTS parameters, factory presets, smoothed gains,
  mix/bypass, a latency-aligned raw-input delay and atomic UI telemetry. It
  accepts matched mono/stereo layouts and has no MIDI input/output.
- `PitchShifter` adapts arbitrary host buffers to preallocated 64-sample
  Signalsmith spectral processing. LIVE uses independent stereo maps;
  QUALITY is an internal preparation option. A delayed clean path and crossfade
  preserve exact unity. Manual pitch and formant histories follow analysis time.
- `PitchDetector` is a filtered/decimated causal YIN-style detector;
  `PitchCorrector` applies key/scale targets, persistence, retune smoothing and
  natural/hard behavior. `Humanizer` preserves measured movement; `BloomMapping`
  changes effective controls without changing their stored base values.
- `PluginEditor` uses JUCE attachments and a 20 Hz message-thread timer. Audio
  publishes telemetry through atomics; UI rendering stays outside processing.
- CMake pins JUCE/Signalsmith dependencies and builds Windows x64 MSVC VST3.
  The regression executable tests production plus isolated ULTRA research code;
  ULTRA is excluded from the shipped plugin. No quality selector is exposed.

## Initial findings and plan (recorded before implementation)

No confirmed P0/P1 defect was found in the supported prepared mono/stereo
lifecycle. This is an audit result, not proof against all possible failures.

| Priority / status | Component and evidence | Impact / proposed action | Risk |
|---|---|---|---|
| P2 verified | `PluginProcessor::setStateInformation` supplies missing defaults for only 14 parameters; missing `value` becomes zero. JUCE fills absent original controls from their current values. | Identical partial states can restore differently; a missing Dry/Wet value mutes. Supply deterministic defaults, retain legacy Preserve=0, add regression coverage. | Low; incomplete-state recovery only. |
| P2 verified | A standalone build of the actual `PitchDetector.cpp` reports no steady voiced updates for a 1100 Hz sine at 44.1/88.2 kHz; 1099 Hz passes. Strict estimated-Hz bounds reject small interpolation overshoot. | Dropouts at the advertised upper boundary. Add a narrow boundary tolerance and edge regressions without replacing the detector. | Low to moderate; verify range and existing pitch tests. |
| P2 verified | At 1000x800, editor layout leaves Tune controls 17 px high despite requesting 26 px numeric boxes; the geometry test allows 17 px. | Undersized value editing. Reclaim header space at minimum height and assert usable numeric fields. | Low; retain visual identity and default size. |
| P2 verified | ZIP packaging reads historical `phase13/CTest.txt` and replaces the shared checksum file; installer lacks a current regression gate. | Stale evidence and lost installer checksum entries. Repair bounded packaging behavior where verifiable; keep installer release-gate work explicit. | Moderate; do not overwrite existing distribution artifacts. |
| P2 measurement gap | Existing production correction timing covers only 64/256 buffers, and historical measurements show deadline misses. | Measure 44.1/48/96 kHz at 64/128/256/512 samples, mono/stereo, and automation before claiming performance changes. | Low; test-only work. |
| P1 known limitation, not a newly established regression | LIVE's 48 ms window plus 64-sample adapter gives approximately 49 ms plugin delay. Weak fundamentals and dominant harmonics can cause octave errors. | Significant monitoring delay and difficult-vocal tracking remain. Retain working engine; evaluate alternatives against a vocal corpus before adoption. | High for an engine redesign. |
| P3 pending validation | Host DPI/keyboard interaction, named-DAW sessions, listening and clean-machine installation lack current acceptance evidence. | Keep these manual release gates explicit. No new controls or broad UI redesign. | Varies. |

Callback inspection found bounded scratch/history processing, no plugin-owned
callback allocation, locking, file I/O or logging. Incoming NaN/Inf is sanitized.
Finite extreme inputs are not limited. Prepare/reset/layout changes still require
host serialization. Presets publish individual parameter changes, not atomic
transactions. Allocation tests cover intercepted C++ allocations, not every OS
allocator. Pitch feedback intentionally stops when Correction is zero.

## Verification and implementation results

The original `build/` cache points to a former project path. This audit uses
`build/audit/`, local dependencies, the existing CMake 3.31.8 tool, MSVC
19.29.30148 x64 and Windows SDK 10.0.19041. No tools were installed.

Implemented changes:

- `Source/PluginProcessor.cpp`: defaults for absent original parameters and
  missing values; legacy absent Formant Preserve remains zero. New stability
  tests restore the same partial state into fresh and modified instances, check
  all 18 parameters, missing-value defaults and wrong-root rejection.
- `Source/dsp/PitchDetector.cpp`: two additional low-period search neighbours
  prevent truncated interpolation (the baseline could misreport 64 Hz as 65 Hz).
  An estimation margin under nine cents admits inclusive endpoints and clamps
  published estimates to 65–1100 Hz. This deliberately tolerates a very narrow
  band outside the nominal range; it is not a brick-wall frequency classifier.
  Interior thresholds, window/hop sizes and the correction/shifting engine stay
  unchanged. `Tests/CorrectionTests.cpp` adds 96 combinations spanning four
  rates, both detector profiles, pure/harmonic signals and outside-range guards.
  The standalone boundary regression failed on original source and passed with
  the fix. FastExperimental remains research-only.
- `Source/PluginEditor.cpp` and `Tests/UiTests.cpp`: reclaim 36 px of decorative
  header space at 1000x800 so Tune value fields retain 26 px height. Tests inspect
  actual editable value-field bounds. Default 1120x900 layout is retained.
  Actual JUCE component snapshots at minimum/default sizes were visually checked;
  this is not host/DPI/hardware validation.
- `tools/PackageRelease.ps1`: configurable build/tool locations, current CTest
  invocation with failure/no-tests rejection, module hashes checked before/after
  testing and copying, and preservation of unrelated checksum entries. The audit
  report is included in ZIP staging. A simulated exit-7 test runner was rejected
  before any package directory was created. System execution policy was not
  changed; scripts were run with a process-scoped `-ExecutionPolicy Bypass`.
- `Tests/AuditBenchmark.cpp`, CMake and test entry point: opt-in
  `--audit-benchmark` measures the actual processor in 96 configurations. No new
  production dependency, parameter or control was added. README and manual
  checklist now describe this workflow and the remaining host checks.

The original Release build and original aggregate CTest passed (136.70 s).
The updated Release build succeeded. CMake regeneration emitted warnings that
`CMAKE_GENERATOR_PLATFORM` was ignored without `CMAKE_GENERATOR`; no compiler
error occurred. Focused UI, correction and stability tests passed. The optional
Phase 7 binary comparison was explicitly skipped because that bundle is absent.
The final aggregate CTest passed (1/1, 287.37 s), including JUCE VST3 discovery,
loading, processing, version/parameter-count checks and the full DSP/state/UI
regressions. Suite wall times are not controlled before/after CPU benchmarks.
ZIP staging used that current test pass; all 34 archived files matched staged
SHA-256 values, the per-file manifest validated, and the unrelated checksum
fixture survived. This was an isolated packaging check under `build/audit`, not
a new installer or published release. Existing `dist/` artifacts are unchanged.

Evidence: [full test output](measurements/audit-2026-10-18/FullTests.txt),
[Release build](measurements/audit-2026-10-18/ReleaseBuild.txt),
[CTest and package results](measurements/audit-2026-10-18/CTestAndPackage.txt),
[CPU matrix](measurements/audit-2026-10-18/cpu.csv).
The rebuilt VST3 module SHA-256 is
`E57320630C24B4D1E781598237B2DDF018DD984EDDA9E156D81036AB9969684A`.

Synthetic correction results: worst steady detector error in the existing
interior-frequency fixture was 1.87539 cents; white-noise voiced frames were
0/375. Rendered 5 Hz vibrato's 5th–95th percentile span was 0.401888 semitones
for natural versus 0.0698895 for hard correction. These demonstrate distinct
control behavior, not subjective human vocal quality.

### Latency and CPU

Latency checks exercise unity/impulse, variable-buffer dry/wet and both bypass
paths, stereo timing and internal QUALITY. The production delay is unchanged:

| Rate | LIVE samples | Milliseconds |
|---|---:|---:|
| 44.1 kHz | 2180 | 49.4331 |
| 48 kHz | 2368 | 49.3333 |
| 88.2 kHz | 4297 | 48.7188 |
| 96 kHz | 4672 | 48.6667 |

The existing `--benchmark live48_16x` probe independently confirmed reported,
clean-impulse and spectral-unity-plus-adapter delay were identical at
44.1/48/96 kHz: 2180/2368/4672 samples. See
[spectral timing](measurements/audit-2026-10-18/spectral-latency.csv).
Its separate shifter-only workload also recorded one 48 kHz/64 callback overrun;
zero misses in a particular matrix run is not a guarantee. Generated synthetic
audition WAVs were not listened to.

Current offline study: Intel Core i5-13600K, Release x64, 0.5 s warm-up then
2 s measured synthetic notes/vibrato/breath per configuration. Mono/stereo,
64/128/256/512 buffers, unity/Natural/Hard Tune/50 ms automation changes.
Timing excludes synthesis, input copies, parameter notifications and output
validation. CPU percent means callback wall time divided by audio duration,
not Task Manager CPU. No concurrent build or audio test was run during this
study; scheduling/background activity remains uncontrolled. No before/after
speedup is claimed, and this is not ASIO or named-host deadline certification.

| Rate | Active mean CPU range, all tested channels/buffers/styles | Active callback deadline misses |
|---|---:|---:|
| 44.1 kHz | 3.62–11.55% | 0 |
| 48 kHz | 6.25–12.02% | 0 |
| 96 kHz | 11.73–23.56% | 586 |

For stereo Hard Tune at 96 kHz/64, 178 of 3000 callbacks exceeded the 666.7 µs
deadline (p99 911.7 µs, max 1350.1 µs). Stereo/128 also had occasional misses;
one mono automation/256 callback missed. No misses occurred at 512 in this run.
The unity path still runs the spectral engines to maintain uninterrupted history.
These results confirm a small-buffer performance limitation rather than justify
claiming real-time safety from average CPU alone.

### Remaining roadmap and release assessment

**Release candidate; manual acceptance remains required.** Successful builds and
synthetic tests do not establish production readiness. No named DAW, ASIO device,
human listening session or clean-machine install was tested during this audit.
Pluginval was not found on PATH or in the checked local tools; the existing JUCE
VST3 discovery/load/process smoke test is retained, not described as pluginval.

| Priority | Next work | Benefit | Complexity / risk |
|---|---|---|---|
| P1 | Profile spectral work distribution at 96 kHz/64–128; compare bounded scheduling changes before adopting them. | Fewer live dropouts. | Medium–high; changing FFT scheduling can alter latency/audio. |
| P1 | Evaluate dominant-harmonic octave handling and voiced/consonant transitions with labelled human recordings. | Better tracking and natural/hard vocal quality. | High; existing reports show severe second-harmonic ambiguity, and a blanket octave heuristic can damage correct notes. |
| P1 | Complete listening and FL Studio/Ableton/REAPER/Studio One save/reopen, bypass and automation checks. | Evidence of actual user/host compatibility. | Medium effort; no compatibility claims until executed. |
| P2 | Reuse the current-test/hash gate in the installer helper; unify version/build paths and verify clean install/upgrade/uninstall. | Safer release preparation. | Medium; existing installer and dist artifacts were deliberately untouched. |
| P2 | Validate a more compact layout and keyboard/screen-reader behavior at Windows display scales. | Broader screen/accessibility support. | Medium; current minimum remains 1000x800. |
| P3 | Fix remaining source-relative documentation links in binary packages. | Clearer installation documentation. | Low. |

Further latency reduction or a different production engine requires separate
quality, CPU and host-delay validation. No speculative engine rewrite or new
feature was introduced. Existing distribution artifacts, plugin identity,
parameter IDs and version 1.0.0 are retained; the audited source is not published.
