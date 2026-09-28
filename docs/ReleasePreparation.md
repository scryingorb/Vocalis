# Vocalis 0.9.0 prototype - release audit

## Scope and fixes

Phase 9 adds no DSP features and preserves all 18 parameter IDs/ranges/defaults,
plugin identity, preset values and normal finite-input sound. Small fixes:

- Replace non-finite incoming samples with zero before either delay or spectral
  processing; one bad upstream sample no longer poisons persistent history.
- Saturate the voiced-acquisition counter at two instead of incrementing forever.
- Validate imported state values for finiteness and clamp/snap to parameter ranges.
- Reuse the existing missing-parameter migration helper for Pitch.
- Complete dependency notices and document the actual runtime imports.

## Audit findings

| Area | Finding |
| --- | --- |
| Callback safety | Bounded 64-sample stack scratch; vectors/FFT plans allocated in prepare; no callback locks, file/network access, logging or component access. Allocation interception covers C++ new, arrays and aligned allocation, not every OS allocator. |
| Smoothing | Gains/mix/bypass/unity crossfade 20 ms; manual pitch/formants 50 ms; humanization controls 50 ms at detector cadence; correction uses nonzero retune/release time constants. Key/scale are discrete target choices. |
| Reset | Preallocated histories cleared; smoothers reinitialized; detector and meter telemetry cleared. Reset must be host-serialized with processing, as must prepare/release and bus changes. |
| Channels/rates/buffers | Mono and stereo matched buses; stereo shared tuning reference is left input. Same-instance prepare tested across all four rates and mono/stereo, including returning to 44.1 kHz. Empty, 1, 17, 64, 128, 256, 511, 1024 and 4096 sample callbacks exercised. |
| Latency | Constant with pitch, presets, mix and bypass; aligned dry and clean bypass delay; impulse/null and stereo drift regressions retained and extended to 88.2 kHz. |
| State/presets | APVTS serialization/migration; all parameter IDs retained. State copy/replace are non-realtime operations, never invoked by our callback. Preset gestures remain message-thread operations; hosts may publish independent atomic parameter changes between callbacks. Preset changes are not an atomic 18-value transaction. |
| Automation | Cached atomic reads and smoothed continuous controls. Concurrent host-style preset/state updates with active audio and editor attachments stress-tested; no observed crash/allocation/non-finite output. This is not a formal race-detector proof. |
| Denormals/gain | ScopedNoDenormals surrounds rendering; subnormal input exercised. No limiter is added: combined Input and Output +24 dB can intentionally clip downstream. Preserve headroom. |
| UI | Message-thread 20 Hz polling, atomic telemetry only; no audio-thread component calls. Meter reset/store race can lose a display peak, never changes audio. |

## Measured evidence

Release MSVC x64 tests include all previous DSP, state, allocation, UI and VST3
host regressions plus Tests/StabilityTests.cpp. New stress matrix covers every
preset at each rate in mono/stereo, mix/bypass changes, fractional and extreme
pitch, formants, tuning, Humanize and Bloom, then reset/silence and NaN/Inf input.

| Rate | Reported samples | Milliseconds |
| --- | --- | --- |
| 44100 | 2180 | 49.4331 |
| 48000 | 2368 | 49.3333 |
| 88200 | 4297 | 48.7188 |
| 96000 | 4672 | 48.6667 |

Standalone stress run: max absolute output 0.0603825 for input peak <=0.06;
max adjacent sample step 0.0523469. Gross discontinuity threshold is 0.25, gain
threshold 0.5; these detect major failures, not every perceptible click. Existing
tone/automation, timing, spectral and preset tests provide complementary checks.
No non-finite output or callback C++ allocation was observed. Source NaN/Inf tests
are distinct from legitimate finite audio over 0 dBFS, which remains unclipped.

The Phase 7 historical binary comparison remains sample-exact for the tested
normal signals with the Phase 9 fixes. It is optional on fresh source checkouts
without the local historical binary. See archived Phase9Tests.txt for final run.

## Distribution and remaining validation

Package the entire outer Vocalis.vst3 directory, including resources/licenses,
with README.md, THIRD_PARTY_NOTICES.md, LICENSES and docs. tools/PackageRelease.ps1
creates a ZIP with these files and SHA256SUMS.txt; no PDB, test executable, SDK,
source tree or CMake tool is required on a listener's computer.

Binary imports were checked with MSVC dumpbin /dependents. Microsoft Visual C++
v14 x64 Redistributable is required (2019 toolset build); use Microsoft's current
supported package for the destination Windows version. JUCE/Signalsmith need no
separate DLL. Remaining imports are Windows system DLLs. Runtime installer is
not bundled. A clean-machine install has not been performed.

This is an unsigned prototype, not a certified commercial release. No new real
vocal listening, actual ASIO underrun measurement, interactive FL Studio automation,
.flp reopening, or multi-host certification was performed. User-reported earlier
FL Studio success is not a substitute for those release checks. At 49 ms plugin
latency, comfortable live monitoring is still not achieved. Small ASIO buffers can
miss deadlines at high rates; see earlier CPU measurements.

Before public distribution, choose the Vocalis code/assets licence and confirm
JUCE licence compliance. Included notices do not resolve that decision. See
THIRD_PARTY_NOTICES.md and the official JUCE licence linked there.

Final full Release suite: 100% passed, 0 failed, 118.37 seconds. Evidence: [full output](measurements/Phase9Tests.txt), [standalone stress output](measurements/Phase9Stability.txt), [binary imports](measurements/Phase9RuntimeImports.txt).
