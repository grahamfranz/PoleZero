# PoleZero

![PoleZero z-plane and magnitude UI](docs/screenshot.png)

A nonlinear z-plane filter plugin. The user drags poles and zeros around the
unit circle on an interactive z-plane; the plugin runs **two** independent
biquad stages, each with its own conjugate pole pair and conjugate zero pair,
combined either in **Series** (cascade) or in **Sum / Difference** (parallel,
Hordijk twin-peak style — the difference of two close resonant peaks gives a
strong mid-frequency notch). The interesting part: a switchable
**boundary condition** is applied per sample inside each stage; when a pole
sits near or past the unit circle the linear recursion would diverge — the
boundary condition catches the runaway and folds it back into the filter,
and that nonlinear feedback is what gives each mode its character.

Formats: **AU**, **VST3**, **Standalone** (macOS + Linux from CI).

## Two stages

Each stage owns one conjugate pole pair and one conjugate zero pair (four
biquad coefficients). The handles are colour-coded on the z-plane:

| Stage | Pole marker (×)             | Zero marker (○) | State trail |
|-------|-----------------------------|-----------------|-------------|
| A     | orange → bright yellow at r≥1 | amber           | peach       |
| B     | cyan → pale ice at r≥1      | lavender        | violet      |

The palette is warm-vs-cool rather than red-vs-green so the stage
distinction holds up under any common form of colour vision deficiency.
The runaway tint is also a brightness shift, so it stays visible regardless.

Stage B defaults to the origin (all radii = 0), which makes it an identity
filter. With the default routing (Series), the plugin then sounds exactly
like the single-stage version — existing presets load unchanged. Drag any of
the stage-B markers off the origin to wake the second stage up.

### Routing

| Mode       | Combine                   | Sound                                                 |
|------------|---------------------------|-------------------------------------------------------|
| Series     | `B(A(x))`                 | Four-pole / four-zero cascade. Two stacked resonances |
| Sum        | `A(x) + B(x)`             | Twin peaks reinforce; broader resonant region         |
| Difference | `A(x) − B(x)`             | Twin peaks subtract; deep notch between them (Hordijk twin-peak) |

The auto-normalisation evaluates the *combined* response at `ω = π/2` and
divides it out so the output gain control stays meaningful regardless of
routing.

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

## BC tap

The `BC Tap` parameter selects *where* in the biquad the boundary condition
bites. Both stages share the tap (along with the BC mode and level) — the
stages differ only in their poles, zeros, and conjugate lock. The same
Clip / Foldover / Modulo / Tanh shape applies; only the signal it shapes
changes.

| Tap     | Shaped signal                                  |
|---------|------------------------------------------------|
| Output  | `y = b0·x + z1` before feedback (original)     |
| State   | The next-step state memories `z1`, `z2`        |

Both taps catch `r ≥ 1` runaway because both sit inside the recursion.
`State` keeps the audible output linear and folds the runaway *inside* the
recursion — the same family of sounds as `Output` but with softer edges and
more internal mixing between the two state memories.

## Feedback

The `Feedback` parameter is a one-sample global recursion around the routing
combine — `y_n = route(x_n + k · y_{n-1})`. At `k = 0` it does nothing. As `k`
climbs toward 1 the loop colours the response with self-resonance; past
`k ≈ 1` it tips into self-oscillation, and the in-stage boundary condition is
again the thing keeping the output bounded. With the BC on `Tanh` you get
soft squelch; on `Clip` or `Modulo` the oscillation takes on the BC's
geometry. Pair it with a pole near or past the unit circle and the system
hums in the chosen boundary's voice.

## State trajectory

The z-plane plots a fading trail of each stage's internal state `z1` as audio
runs (stage A in violet, stage B in peach). It's a phase portrait: stable
filters spiral to the origin, ringing pole pairs trace ellipses, and
BC-bounded runaway draws the closed orbit the chosen boundary map locks the
system into. Swap the BC mode, the tap, or the routing while playing and the
orbit shapes change immediately.

## Parameters

| ID                | Range                          | Notes                                            |
|-------------------|--------------------------------|--------------------------------------------------|
| `pole1Radius`     | 0 – 1.6                        | Stage A primary pole magnitude                    |
| `pole1Angle`      | -π – π                         | Stage A primary pole angle                        |
| `pole2Radius`     | 0 – 1.6                        | Stage A secondary pole; locked to conj(pole1) by default |
| `pole2Angle`      | -π – π                         |                                                  |
| `zero1Radius`     | 0 – 1.6                        | Stage A primary zero magnitude                    |
| `zero1Angle`      | -π – π                         |                                                  |
| `zero2Radius`     | 0 – 1.6                        | Stage A secondary zero; locked to conj(zero1) by default |
| `zero2Angle`      | -π – π                         |                                                  |
| `lockConjugate`   | bool                           | Stage A: mirror secondary as conjugate of primary |
| `poleB1..2Radius` | 0 – 1.6                        | Stage B pole pair (default 0 → identity)          |
| `poleB1..2Angle`  | -π – π                         | Stage B pole angles                               |
| `zeroB1..2Radius` | 0 – 1.6                        | Stage B zero pair (default 0)                     |
| `zeroB1..2Angle`  | -π – π                         | Stage B zero angles                               |
| `lockConjugateB`  | bool                           | Stage B: mirror secondary as conjugate of primary |
| `routing`         | Series / Sum / Difference      | How the two stages combine                        |
| `boundary`        | Clip / Foldover / Modulo / Tanh | Boundary shape (shared)                          |
| `boundaryTap`     | Output / State                 | Where the BC bites (shared between stages)        |
| `boundaryLevel`   | 0.3 – 4.0                      | BC threshold (shared); ≈ amplitude where it bites |
| `feedback`        | 0.0 – 1.2                      | One-sample global feedback around the routing combine |
| `outputDb`        | -24 – +24 dB                   | Output trim after linear auto-normalisation       |

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

### Running unsigned builds on macOS

The release zips contain unsigned AU, VST3, and Standalone bundles. On first
launch macOS Gatekeeper will refuse to load them. Clear the quarantine
attribute after copying them into place:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/PoleZero.component
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/PoleZero.vst3
xattr -dr com.apple.quarantine /path/to/PoleZero.app
```

Or build from source — the local toolchain ad-hoc signs the artefacts and the
warning doesn't appear.

## Files

```
CMakeLists.txt
LICENSE                           GPLv3
.github/workflows/build.yml       CI: build + release on tag
Source/
  BoundaryCondition.h             the four boundary maps + BC tap enum
  PoleZeroFilter.h                complex biquad with switchable BC tap + safety clamp
  TrajectoryProbe.h               SPSC ring buffer of filter state samples
  PluginProcessor.{h,cpp}         APVTS + audio thread, two-stage routing
  PluginEditor.{h,cpp}            controls strip
  ZPlaneComponent.{h,cpp}         draggable z-plane, 8 handles, two state trails
  MagnitudeResponseComponent.{h,cpp} linear |H(e^jw)| of the combined response
```

## License

GPLv3 — see [LICENSE](LICENSE). PoleZero links JUCE, which is itself GPLv3
unless you hold a commercial JUCE license. Distributed binaries inherit those
terms.
