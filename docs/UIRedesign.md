# Phase 8: Frutiger Aero interface

The editor now uses translucent mint glass panels, aqua accents, soft reflections,
glossy bubble knobs and pill-shaped preset buttons. VOCALIS and Vocal Shaping
Engine remain the brand. The central Pitch Orb sits above TUNE, VOICE and HUMANIZE;
the nine preset capsules and Input / Dry/Wet / Output / Bypass sit underneath.
Note Transition remains accessible in TUNE alongside the requested controls.

## Components and resizing

`Source/ui/VocalisTheme.h` centralizes palette, fonts, spacing, corner radii,
gradients, reflections and soft shadow drawing. `VocalisLookAndFeel` applies the
shared style to rotary/linear sliders, combo boxes, menus and tooltips.
`BubbleKnob`, `BubbleButton`, `PresetCapsule`, `PitchOrb`, `SectionPanel`,
`LevelMeter` and `BloomControl` are reusable components in `Source/ui/`.

`PluginEditor` uses shared grid-cell and labelled-control calculations, relative
panel geometry and JUCE resize constraints. Default size is 1120 x 900 logical
pixels; limits are 1000 x 800 through 1600 x 1200. All 18 parameter controls remain
attached to the existing APVTS parameters. Presets still use the existing manager;
a matching preset glows, and manual edits remain possible (otherwise Custom).
Double-click restores a parameter's default; editable numeric fields and tooltips
remain available.

The orb displays the existing detector's INPUT note and signed cents relative to
A4 = 440 Hz, e.g. A3 / -07. It shows -- when correction/detection is inactive or
unvoiced. It does not simulate detection or claim to show corrected output pitch.
The screenshots inject a known test frequency into the display component only.

## Audio and UI separation

No parameter IDs, ordering, ranges, defaults, preset values, state schema or DSP
algorithms changed. SHA-256 comparison confirmed all 13 files under Source/dsp
and Source/presets are byte-identical to the pre-redesign versions.

The only callback additions are input/output absolute peak accumulation and
lock-free atomic telemetry stores. Meters consume those values on the message
thread. They do not feed audio processing; the display may miss an occasional
peak during a producer/consumer race. These are lightweight visual peak meters,
not calibrated loudness or true-peak meters. No callback locks, allocations,
logging, file access or component access were added.

One message-thread timer polls at 20 Hz. Orb text repaints only when its displayed
note/cents change, meters decay and repaint when their level changes, and preset
buttons update when their matching state changes. Glow is static; there is no
continuous animated background or audio-thread drawing.

## Verification

Release VST3 and test targets are built with MSVC for Windows x64.
`Tests/UiTests.cpp` checks all 27 interactive controls at 1000 x 800, 1120 x 900,
1600 x 1200 and 1450 x 850: controls stay within the editor and do not overlap.
It checks real note/cents conversion and inactive display, and exports snapshots.
The minimum, default and maximum layouts were visually inspected.

A saved pre-redesign Phase 7 VST3 in build/phase8-baseline/Vocalis.vst3 is loaded
through JUCE's VST3 host. Across all nine presets at 44.1, 48 and 96 kHz, the new
processor with its editor open produces sample-exact output versus that binary
for stereo harmonic signals (100 blocks of 511 samples per case). Host latency
also matches. Tested new-processor callbacks allocate no C++ memory. This is a
synthetic regression comparison, not a new subjective listening evaluation.
The historical binary comparison explicitly skips on clean checkouts where that
local baseline is absent; layout and the existing DSP tests still run.

The full suite retains mono/stereo, variable-buffer, state restore, automation,
pitch/formant/correction/humanizer/Bloom, all-presets, dry/wet, bypass, allocation
and VST3 loading checks. Logs: build/phase8-build.log, build/phase8-ui-tests.log,
and build/phase8-tests.log. Snapshots: build/Vocalis-ui-WIDTHxHEIGHT.png.

Latency remains LIVE: 2180 samples at 44.1 kHz, 2368 at 48 kHz and 4672 at 96 kHz
(about 49 ms). This UI change makes no latency or audio-quality claim.

FL Studio interaction itself has not been exercised in this automated pass.
After installing, reopen an existing project, confirm parameter automation and
preset recall, resize the editor, edit numeric values and verify the Pitch Orb
with correction enabled on a vocal. Existing README build/install steps apply.

Updated bundle: build/Vocalis_artefacts/Release/VST3/Vocalis.vst3.

Final Release CTest result: 100% passed, 0 failed (103.18 seconds). Archived full output: [Phase8Tests.txt](measurements/Phase8Tests.txt). Preview: [Phase8UI.png](measurements/Phase8UI.png).
