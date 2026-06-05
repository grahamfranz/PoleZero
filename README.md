# PoleZero

A nonlinear z-plane filter plugin. The user drags a pole and a zero around the
unit circle on an interactive z-plane; the plugin runs a biquad with a
conjugate pole pair and conjugate zero pair. The interesting part: a switchable
**boundary condition** is applied to the filter's feedback sample every step.
When the pole sits near or past the unit circle the linear recursion would
diverge — the boundary condition catches the runaway and folds it back into
the filter, and that nonlinear feedback is what gives each mode its character.

Formats: **AU**, **VST3**, **Standalone** (macOS + Linux from CI).

## Boundary conditions

All four are applied per-sample to the filter's feedback signal, bounded at
`±Level`:

| Mode      | Shape                              | Sound                                   |
|-----------|------------------------------------|-----------------------------------------|
| Clip      | hard clip at `±L`                  | square-edged self-oscillation           |
| Foldover  | triangle wave, period `4L`         | wavefolder feedback, FM-like sidebands  |
| Modulo    | sawtooth wrap, period `2L`         | bitcrush-style discontinuity            |
| Tanh      | `L · tanh(x/L)`                    | Moog-style "warm" saturation            |

Push **Pole Radius** past `1.0` to put the filter in runaway territory — the
boundary condition is what's keeping the output bounded. The pole marker tints
orange as you approach and pass the unit circle.

## Parameters

| ID              | Range            | Notes                                       |
|-----------------|------------------|---------------------------------------------|
| `poleRadius`    | 0 – 1.6          | Conjugate pole pair magnitude                |
| `poleAngle`     | 0 – π            | Conjugate mirrored automatically             |
| `zeroRadius`    | 0 – 1.6          | Conjugate zero pair magnitude                |
| `zeroAngle`     | 0 – π            | Conjugate mirrored automatically             |
| `boundary`      | Clip / Foldover / Modulo / Tanh | Per-sample feedback shaping  |
| `boundaryLevel` | 0.05 – 4.0       | Where the BC bites (≈ amplitude threshold)   |
| `gainDb`        | -24 – +24 dB     | Output trim after linear auto-normalisation  |

The processor evaluates `|H(e^{jπ/2})|` and divides it out so the user's gain
control stays meaningful as the pole moves; the nonlinear BC then shapes the
runaway on top of that.

## Build

PoleZero builds with CMake against a local JUCE source tree (JUCE 8.x).

```sh
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
cmake -S . -B build -DJUCE_PATH=$PWD/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On macOS the AU and VST3 are copied into your user plug-in folders after build
(`COPY_PLUGIN_AFTER_BUILD`). The Standalone app lives under
`build/PoleZero_artefacts/Release/Standalone/`.

### Linux dependencies

```sh
sudo apt-get install -y \
  libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev
```

## Releases

Tag a commit `v*` and push — the CI workflow at `.github/workflows/build.yml`
builds macOS and Linux artefacts and attaches them as zips to a GitHub Release.

## Files

```
CMakeLists.txt
LICENSE                           GPLv3
.github/workflows/build.yml       CI: build + release on tag
Source/
  BoundaryCondition.h             the four boundary maps
  PoleZeroFilter.h                stereo biquad, BC in the feedback path
  PluginProcessor.{h,cpp}         APVTS + audio thread
  PluginEditor.{h,cpp}            controls strip
  ZPlaneComponent.{h,cpp}         draggable z-plane
```

## License

GPLv3 — see [LICENSE](LICENSE). PoleZero links JUCE, which is itself GPLv3
unless you hold a commercial JUCE license. Distributed binaries inherit those
terms.
