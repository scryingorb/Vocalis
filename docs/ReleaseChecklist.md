# Vocalis 1.0.0 Release Candidate checklist

Unchecked items require a human/host or another computer. Do not infer a pass
from an automated test. Record tester, date, host version, OS, interface, driver,
buffer size, preset/settings and any observed failure.

## FL Studio (pending)

- [ ] Close FL Studio and install the complete Vocalis.vst3 bundle.
- [ ] Scan/verify VST3 plugins; confirm Vocalis loads in a Mixer effect slot.
- [ ] Process a recorded vocal and verify Input, Output, Dry/Wet and Bypass.
- [ ] Confirm the green GUI renders correctly at minimum/default/large sizes and
      Windows display scales used on the target machine; inspect values/labels.
- [ ] Load Natural, Pop, Hard Tune, Feminine, Deep, Airy, Robot, Cartoon, Monster.
- [ ] Confirm safe gain/headroom and edit parameters after each preset.
- [ ] Automate Pitch; listen through ramps and rapid changes.
- [ ] Automate Formant; repeat with Formant Preserve changes.
- [ ] Automate Dry/Wet and Bypass; check timing alignment and clicks.
- [ ] Check Key/Scale, Correction, Retune, Transition and Natural/Hard.
- [ ] Check Humanize, Vibrato, Drift, Smoothness and Bloom.
- [ ] Verify the host reports LIVE latency from the README table.
- [ ] Save a project with preset edits and automation; close FL Studio completely.
- [ ] Reopen the project; confirm settings, preset indication and automation.
- [ ] Open an older Vocalis project; confirm identity, settings and sound.
- [ ] Test 44.1/48/88.2/96 kHz and practical ASIO buffers; note dropouts.

QUALITY is tested internally, not selectable in the current plugin. ULTRA is
not shipped. Do not claim to have tested either through an absent UI selector.

## Vocal listening (pending)

Use recorded human voices, moderate input levels and then controlled loud input.
Start at unity Pitch/Formant and compare moderate and extreme transformations.

- [ ] Low vocal
- [ ] Mid vocal
- [ ] High vocal
- [ ] Sustained vowel
- [ ] Vibrato
- [ ] Slide
- [ ] Rapid notes
- [ ] Consonant-heavy passage
- [ ] Breathy vocal
- [ ] Loud vocal

For each, record pitch jumps, octave errors, clicks/pops, metallic artifacts,
flutter, broken consonants, unstable formants, clipping and unexpected gain.
Repeat gentle tuning, Hard Tune, manual pitch/formant, Humanize and Bloom.
Aligned offline exports cannot validate live monitoring delay. Total monitoring
latency includes interface input, ASIO/host buffers, plugin and interface output.

## Clean Windows x64 computer (NOT performed)

- [ ] Use another Windows 10/11 x64 machine without development dependencies.
- [ ] Install supported Microsoft Visual C++ v14 x64 Redistributable if required.
- [ ] Verify ZIP SHA-256 against dist/SHA256SUMS.txt from the delivery.
- [ ] Copy complete Vocalis.vst3 to C:\Program Files\Common Files\VST3.
- [ ] Open a Windows x64 VST3 host and rescan/verify plugins.
- [ ] Load Vocalis; verify all GUI resources and resizing.
- [ ] Verify mono/stereo audio and latency reporting.
- [ ] Verify all presets and manual edits.
- [ ] Save, close and reopen a project; verify state and automation.
- [ ] Confirm no missing-runtime errors, startup crashes or resource failures.

## Public release gates (pending)

- [ ] Owner confirms JUCE licensing entitlement/route and associated obligations.
- [x] Owner supplied proprietary Vocalis terms in LICENSE.txt.
- [ ] Confirm bundled notices suit the actual distribution/licensing route.
- [ ] Complete FL Studio, human listening and clean-machine checks above.
- [ ] Review known limitations and decide whether any findings block release.

This release candidate is unsigned. No public-release or commercial clearance
is asserted by successful compilation or inclusion of licence files.

