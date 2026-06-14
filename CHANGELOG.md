# Changelog

All notable changes to PoleZero are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Build & distribution

- CI matrix now includes `windows-latest` alongside `macos-latest`
  and `ubuntu-latest`. Future tagged releases will ship Windows VST3
  and Standalone artefacts in addition to macOS and Linux.
- README now carries a CI status badge.

## [0.1.0] — 2026-06-13

First public release.

### Plugin

- Two-stage z-plane filter; each stage is one conjugate pole pair plus one
  conjugate zero pair (four biquad coefficients per stage).
- Routing modes: **Series** cascade, **Sum**, and **Difference**
  (Hordijk twin-peak notch).
- Pole and zero radii range to 1.6 — deliberately past the unit circle.
- Boundary Condition shapes: **Clip**, **Foldover**, **Modulo**, **Tanh**.
- **BC Tap**: Output or State — per-sample shaping location inside the
  recursion.
- One-sample global **Feedback** around the routing combine.
- Per-stage **conjugate-lock** toggle.
- 4× internal oversampling on the audio path
  (2 polyphase IIR stages).
- Linear auto-normalisation against |H(e^{jπ/2})|, with an Output dB trim
  applied after.

### UI

- Interactive z-plane with eight draggable handles, colour-coded per stage
  (warm for A, cool for B; tints brighten as `r → 1`).
- State trajectory trail — phase portrait of each stage's `z1`.
- Linear magnitude-response plot of the combined route.
- Centre-weighted radius mapping for fine control near the unit circle.

### Build & distribution

- CMake against an external JUCE 8.0.4 source tree.
- Formats: AU, VST3, Standalone.
- GitHub Actions CI builds macOS and Linux on push to `main` and on PRs;
  tagged `v*` commits trigger a release with per-platform artefact zips.
- Licensed under GPLv3.

[Unreleased]: https://github.com/grahamfranz/PoleZero/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/grahamfranz/PoleZero/releases/tag/v0.1.0
