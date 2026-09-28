# Phase 2.5 latency investigation

Release 0.2.5 defaults to **LIVE: 48 ms window, 3 ms hop, split computation off**.
The Phase 2 configuration is retained as internal QUALITY. No formants, tuning,
detection, presets or new user parameters were added.

## Original latency and new architecture

Phase 2 used manual `configure(channels, sampleRate * 0.12, sampleRate * 0.015)`.
Neither `presetDefault()` nor `presetCheaper()` was used. The omitted split flag
defaulted to false. At equal input/output rates, analysis plus synthesis latency
equals the configured window length. The adapter adds 64 samples. At 48 kHz this
was `5760 + 64 = 5824`, or 121.333 ms.

LIVE uses 48 ms / 3 ms instead. Seconds are truncated to samples as in Phase 2.
The 64-sample adapter, 50 ms pitch ramp and 20 ms gain/mix/bypass/unity crossfades
remain. Ramps affect control response, not fixed audio latency.

`PitchShifter::Mode`, `Configuration` and `configurationFor()` separate preparation
from processing. Default `prepare()` selects LIVE; `Mode::quality` retains the
old settings. A future selector must suspend/reprepare and update host latency,
not reconfigure FFT engines inside a callback. No selector or automated mode
parameter is added here. APVTS IDs, VST3 identity and saved-state compatibility remain.

Short windows with dense overlap exposed shared spectral-peak interactions:
unrelated 220/330 Hz left/right tones shifted incorrectly with linked processing.
LIVE uses two deterministic mono engines sharing the same frame clock, pitch
trajectory and delay. Tests verify identical-channel equality, common timing and
coherent-tone phase stability over time. **Independent maps can change the stereo
phase offset and image when shifting**; they do not preserve the original image
as linked processing does. QUALITY retains linked stereo. There is no extra
left/right delay, and zero pitch remains an exact delayed copy.

## Actual latency

Impulse peaks were measured against reported latency. The spectral engine was
also exercised independently at unity, so this checks more than the wrapper's
clean delay. All dry/wet and bypass timing/null checks pass.

| Rate | Previous samples / ms | LIVE block / hop | New samples / ms | Reduction |
| --- | --- | --- | --- | --- |
| 44,100 | 5,356 / 121.451 | 2,116 / 132 | 2,180 / 49.433 | 59.3% |
| 48,000 | 5,824 / 121.333 | 2,304 / 144 | 2,368 / 49.333 | 59.3% |
| 96,000 | 11,584 / 120.667 | 4,608 / 288 | 4,672 / 48.667 | 59.7% |

`setLatencySamples()` still receives `inputLatency() + outputLatency() + 64`
during preparation. Dry, bypass and wet-unity paths use the same delay, even
at zero pitch. Latency is not disabled or dynamically shortened at bypass.

## Configuration comparison

Table: 48 kHz, stereo, 64-sample caller buffer. CPU is DSP adapter execution time
divided by audio duration, as percent of one core's realtime budget, **not** total
Windows CPU or the FL Studio meter. Tone errors are worst across 44.1/48/96 kHz,
80/110/220/330 Hz and 0/+3/-5.5/+12/-12 st. Stereo errors use unrelated 220/330 Hz
tones. One cent is 1/100 semitone. These are measured results, not listening scores.

| Configuration | Block samples (ms) | Hop / overlap | Latency ms | CPU | Mono / stereo error, cents | Decision / expected tradeoff |
| --- | --- | --- | --- | --- | --- | --- |
| Original / QUALITY, linked | 5,760 (120) | 720 / 8x | 121.33 | 1.95% | 16.5 / 6.8 | Retained; finer frequency resolution, high delay |
| 60 ms, linked | 2,880 (60) | 360 / 8x | 61.33 | 1.94% | 37.3 / 42.4 | Lower delay, worse stereo pitch accuracy |
| 40 ms, linked | 1,920 (40) | 240 / 8x | 41.33 | 1.89% | 70.9 / 63.7 | Rejected pitch accuracy |
| 30 ms, linked | 1,440 (30) | 180 / 8x | 31.33 | 1.92% | 122.1 / 496.8 | Rejected severe low-note/stereo errors |
| 20 ms, linked | 960 (20) | 120 / 8x | 21.33 | 1.88% | 158.5 / 505.9 | Rejected; too few cycles of low vocal notes |
| 40 ms, linked + split | 1,920 (40) | 240 / 8x | 46.33 | 1.91% | 70.9 / 63.7 | Adds 5 ms, same pitch limitations |
| 40 ms, independent | 1,920 (40) | 60 / 32x | 41.33 | 8.93% | 27.3 / 6.3 | Viable tones, expensive at high rates/small buffers |
| **LIVE, independent** | **2,304 (48)** | **144 / 16x** | **49.33** | **4.89%** | **33.7 / 8.9** | Selected balance; accuracy/image tradeoff |

Other 40/48/60/80 ms and 4x/16x/32x/64x overlap variants are in the CSV evidence.
More overlap is not always better: extreme overlap worsened some peak maps while
raising CPU substantially. No candidate was selected solely for minimum latency.

## Split computation

The pinned source and [upstream documentation](https://github.com/Signalsmith-Audio/signalsmith-stretch#split-computation)
show that split computation adds one hop to output latency. Representative
40 ms/5 ms and 40 ms/1.25 ms split variants were benchmarked. Split reduced some
p99 times but did not reduce mean CPU or pitch error, or eliminate scheduling
outliers. LIVE leaves it off to avoid another 3 ms with the selected hop.

## CPU and deadline measurements

Hardware: Intel Core i5-13600K. Windows, MSVC 19.29 Release, Signalsmith Linear's
built-in FFT. Each row processes three seconds of stereo synthetic voiced
harmonics/vibrato plus low noise. Source generation, preparation, allocation and
file writes are outside timing. Calls run offline, without pinned affinity or
realtime scheduling. OS scheduling and hybrid-core placement affect tails.
Processor mixing, GUI, host and driver costs are excluded. This is not a realtime
profiler or an ASIO underrun test.

Final 64-sample-buffer run:

| Rate | QUALITY CPU / p99 | LIVE CPU / p99 | LIVE maximum call | LIVE deadline overruns |
| --- | --- | --- | --- | --- |
| 44.1 kHz | 1.95% / 294.6 us | 4.86% / 183.4 us | 1,015.4 us | 0 |
| 48 kHz | 1.95% / 286.7 us | 4.89% / 181.3 us | 384.6 us | 0 |
| 96 kHz | 4.02% / 586.4 us | 9.99% / 323.2 us | 1,775.8 us | 6 |

At 96 kHz, 64 samples allow only 666.7 us. QUALITY also missed five offline
deadlines in this run. Both modes had zero measured misses at 256 samples for
all rates. An earlier LIVE run recorded one 96 kHz/64-sample miss; it is preserved
in `live-initial-timings.csv` rather than hidden. These are **not measured ASIO
underruns**, but prevent claiming guaranteed dropout-free operation. Start at
256 samples at 96 kHz and verify FL Studio/interface behavior under project load.

LIVE costs roughly 2.5x the original mean stereo CPU while reducing typical FFT
callback spikes. The independent 40 ms/32x option cost 8.9% at 48 kHz and 18.7%
at 96 kHz. The selected 48 ms/16x saves substantial CPU for only 8 ms extra delay.

## Quality evidence and limits

LIVE's worst measured absolute tone error was 33.749 cents versus 16.468 for
QUALITY. Its worst steady-tone variation across measurement windows was 0.105
cents. Regression tones at 220/330 Hz remain within 10 cents. Low 80/110 Hz tests
allow up to 35 cents error and less than 1 cent drift: a deliberate compromise,
not a claim of precise automatic tuning.

A 15 ms Hann-gated 220 Hz burst shifted +3 st had a 90%-energy width of 7.042 ms
in LIVE versus 7.250 ms in QUALITY at 48 kHz (input 6.958 ms). The shifted impulse
centroid closely matched reported delay. These simple transients did not show
severe spreading, but are not comprehensive consonant or plosive tests.

Automated sine pitch/bypass transitions are finite with bounded sample steps.
Broadband dry/wet and both bypass paths retain timing without drift. Identical
stereo channels match; coherent-tone phase is stable over time, though its offset
can change. At zero pitch, the exact clean path prevents comb filtering from
unequal dry/wet delays. At nonzero pitch, interference between original and
transposed audio is expected and cannot generally null.

**No subjective listening assessment on recorded vocals was performed.** Shorter
windows are expected to increase low-note/grainy/metallic artifacts, especially
at extremes. Numeric tests do not prove absence of those artifacts, clicks on
every input, or host underruns. Synthetic audition WAVs under
`build/latency-benchmark/` are not human vocal recordings. Audition sustained
vowels, low notes, sibilants, plosives and stereo reverb tails in FL Studio.

## Realtime safety and validation

No new dependencies. Engines, FFTs and rings are allocated/prepared before audio
processing. No avoidable allocation, blocking locks, file access, logging or UI
access was added in callbacks. C++ allocation hooks cover tone and broadband
mono/stereo processing, automation and reset (new/array/aligned allocations,
not every possible OS allocator).

Release VST3 and CTest pass: 44.1/48/96 kHz; mono/stereo; requested/fractional
shifts; odd/variable buffers 1-4096; block-partition equivalence; APVTS state and
old-state migration; dry/wet/both bypass timing; zero-pitch impulses; coherent
stereo drift; allocation checks; UI rendering; actual VST3 shifting and latency.

Reproduce the benchmark from the `build` directory:

```powershell
& './VocalisTests_artefacts/Release/VocalisTests.exe' --benchmark
# Or run selected candidate names/prefixes:
& './VocalisTests_artefacts/Release/VocalisTests.exe' --benchmark quality120
& './VocalisTests_artefacts/Release/VocalisTests.exe' --benchmark live48_16x
```

Evidence in [measurements](measurements): `candidate-*.csv` is the linked sweep,
`dense40-timings.csv` is the independent 40 ms comparison, `quality-final-timings.csv`
and `live-final-timings.csv` are the final runs. Pitch results are in `live-tones.csv`
and `live-stereo-tones.csv`. Rerunning overwrites build outputs; these report copies
retain the evidence. Custom configurations are preparation/benchmark inputs,
not exposed plugin parameters.

## Monitoring conclusion

The reduction is about 59-60%, with accurate PDC. Around 49 ms plus ASIO/hardware
latency remains noticeable and cannot be called universally comfortable vocal
monitoring. It is a lower-delay creative monitoring compromise; truly low-delay
monitoring remains future DSP work. Keeping LIVE/QUALITY separate is worthwhile
for playback quality, CPU tradeoffs and linked stereo imaging. No Phase 3 work
has started.

Updated bundle:
`C:\Users\Admin\Desktop\Vocalis\build\Vocalis_artefacts\Release\VST3\Vocalis.vst3`.
