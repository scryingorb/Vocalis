# Vocalis 1.0.0 Release Candidate audit

This is the Phase 13 feature-frozen candidate, not a declaration of public release
readiness. No production Source file changed. CMake/product/PE/VST3 metadata now
use 1.0.0; plugin manufacturer/code/bundle identity and all parameter IDs remain.
Historical phase reports retain their original versions and measurements.

## Engine and realtime audit

LIVE remains explicitly selected in prepareToPlay. QUALITY remains functional
through the existing internal PitchShifter preparation API, without a UI switch.
ULTRA sources are test-target-only, not part of the Release VST3. No new features,
DSP algorithms, presets, references or GUI design were introduced.

Callback review confirms bounded 64-sample stack scratch, cached atomic controls,
preallocated histories and no plugin callback file/network access, logging,
blocking locks or UI calls. Allocation/FFT configuration remains in preparation.
ScopedNoDenormals covers rendering. Incoming NaN/Inf samples are zeroed; valid
loud audio is not limited. Continuous controls retain existing smoothing. State
serialization and preset gestures run outside the plugin's realtime callback.
Host lifecycle operations must remain serialized. Presets are independent
parameter changes, not a single atomic transaction. Allocation instrumentation
covers tested C++ allocations, not every possible platform allocator.

Both modes retain accurate fixed latency including adapter buffering. Dry/wet,
unity and bypass paths remain aligned. See README for all four sample rates.
No ULTRA latency is advertised as a product capability.

## Automated verification

Fresh MSVC x64 Release build used clean-first. VST3 discovery now asserts version
1.0.0. Historical sample-exact reference comparison includes 88.2 kHz in addition
to 44.1/48/96 kHz, without changing reference output. The full regression target
covers production and existing isolated research tests. Detailed build, runtime
imports and test evidence live in docs/measurements/phase13 in the source workspace
(the development measurements and test executables are excluded from the ZIP).

Coverage includes mono/stereo, rates, variable/empty buffers, gain/mix/bypass,
pitch/formants/preservation, detector/correction/key/scale/retune/transition/hard,
Humanize/vibrato/drift/smoothness/Bloom, every factory preset, manual edits,
automation, state/malformed state, silence, extreme settings, invalid input,
counter bounds, reset/reprepare, concurrent control updates and callback allocation.
Finite test signals do not prove safety for arbitrary mathematical magnitudes.
Preset gain checks are bounded fixtures, not a limiter guarantee. Long-session
coverage is simulated stress/counter checks, not an overnight real-time host run.

## Runtime and notices

PE imports include MSVCP140, VCRUNTIME140 and VCRUNTIME140_1 plus Windows system
DLLs/UCRT. Install the supported Microsoft Visual C++ v14 x64 Redistributable;
no separate JUCE/Signalsmith runtime is needed. The binary is unsigned.

Bundled JUCE, Signalsmith Stretch/Linear and VST3 SDK licence copies were verified
byte-for-byte against local dependency sources. The notice inventory also retains
FLAC, Ogg/Vorbis, JPEG, PNG, zlib, HarfBuzz and SheenBidi notices, including the
SheenBidi modification record. Bundle resource copies are included.

The owner supplied proprietary Vocalis terms in LICENSE.txt. Owner confirmation of JUCE entitlement/licensing route remains a public-release blocker. Licence files
are not proof of compliance with all source/redistribution obligations.

## Pending acceptance

FL Studio user listening, project reopen/automation, DPI/hardware rendering and
clean-machine testing are not claimed completed. Use ReleaseChecklist.md. Known
limitations include monophonic detector ambiguity, substantial monitoring delay,
extreme-transform artifacts, no released ULTRA, and possible >0 dBFS output on
loud input. Preserve headroom. Production default stays LIVE.

The ZIP contains only the complete VST3 bundle, README, notices/licence files,
release audit/checklist and a per-file checksum manifest. The ZIP and primary
module SHA-256 values are in dist/SHA256SUMS.txt outside the archive. No source,
QA audio, build directories, test binaries or development tools are packaged.
## UI inspection

Minimum 1000x800, default 1120x900 and wide 1450x850 snapshots were visually
inspected: labels, values and preset capsules were readable with no obvious
clipping/overlap. Automated layout checks additionally cover 1600x1200, control
bounds and intersections. No GUI changes were necessary. This does not replace
Windows DPI/host hardware rendering tests in the manual checklist.

## Targeted loud-input regression

The suite now also processes a 2.0-peak tone at +24 dB Input and Output while
switching extreme +/-12 st Pitch and +/-6 st Formant, at all four rates in
mono/stereo. It checks finite/bounded output, callback C++ allocation and exact
silence after reset. Output is intentionally allowed above unity; callers need
headroom and a downstream meter. No successful reference waveform was changed.

## Final result

Clean Release build succeeded with no compiler warnings/errors recorded.
Final CTest: 1 aggregate regression target passed, 0 failed, 139.63 seconds.
It contains multiple parameterized checks; this is not a claim of a separately
counted assertion total. The new loud-input regression and sample-exact production
reference comparison passed. No production Source file changed. Human/FL Studio
and clean-machine acceptance remain pending as explicitly listed above.

