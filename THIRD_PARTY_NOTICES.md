# Third-party notices

Vocalis 1.0.0 Release Candidate uses these compiled-in dependencies. Retain this file and
all files in LICENSES (also shipped inside the VST3 Contents/Resources/Licenses).
This product includes software developed by the Independent JPEG Group.

| Dependency | Revision/version | License / included notice |
| --- | --- | --- |
| JUCE | 8.0.12, 29396c22c93392d6738e021b83196283d6e4d850 | JUCE commercial licence or AGPLv3; JUCE-LICENSE.md |
| Signalsmith Stretch | 1.3.2, a670068d9aeb64913331d5cc29337b19a457a7df | MIT; Signalsmith-Stretch-MIT.txt |
| Signalsmith Linear | 0.6.4, de55e6a50ffcf6f8f43f649692d94691c7025151 | MIT; Signalsmith-Linear-MIT.txt |
| JUCE bundled VST3 SDK | bundled source snapshot | MIT; VST3-MIT.txt |
| FLAC | JUCE bundled | BSD; FLAC-BSD.txt |
| Ogg/Vorbis | JUCE bundled | BSD; Ogg-Vorbis-BSD.txt |
| libjpeg | JUCE bundled | Independent JPEG Group; JPEG-README.txt |
| libpng | JUCE bundled | libpng licence; PNG-LICENSE.txt |
| zlib | JUCE bundled | zlib; zlib-LICENSE.txt |
| HarfBuzz | JUCE bundled | Old MIT; HarfBuzz-LICENSE.txt |
| SheenBidi | JUCE bundled | Apache 2.0; SheenBidi-Apache-2.0.txt and JUCE modification record |

Signalsmith provides spectral pitch and formant processing at unchanged duration.
Both Signalsmith libraries are header-only CMake interface targets. Local external/
copies take precedence; otherwise CMake FetchContent uses the pinned revisions.
No optional FFT backend or separate DSP runtime DLL is used. The JUCE defaults
compile image/font and audio codec support; notices are retained conservatively
regardless of linker dead-code elimination. AU, AAX, LV2, ASIO, OpenGL and mobile
backends are not enabled for this Windows VST3 target.

The final binary dynamically imports the Microsoft Visual C++ v14 x64 runtime.
Install Microsoft's supported redistributable on the receiving machine; it is
not included in the release candidate archive. Other imports are Windows system DLLs.
See https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist .

The permissive DSP licences do not license JUCE or Vocalis itself. JUCE requires
compliance with either the applicable JUCE 8 commercial licence or AGPLv3.
Including notices alone does not satisfy AGPL source obligations. Confirm your
JUCE entitlement before proprietary distribution:
https://juce.com/legal/juce-8-licence/ . No JUCE entitlement was verified here.

Owner-provided proprietary terms for Vocalis are in LICENSE.txt.
The packaged release candidate is a technical delivery, not an assertion that public or
commercial distribution has been cleared. The owner must meet the chosen JUCE licensing route before publication; Vocalis terms do not replace third-party licences.

SheenBidi copyright (C) 2014-2025 Muhammad Tayyab Akram. JUCE's bundled include-path modifications are recorded in LICENSES/SheenBidi-JUCE-CHANGES.txt.


