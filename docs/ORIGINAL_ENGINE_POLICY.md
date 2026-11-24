# Hush Original-Engine and Project-Isolation Policy

Hush-PS2-Engine is an independent PlayStation 2 emulator project. Its implementation must be authored as Hush code and must not contain, vendor, bundle, wrap, rename, or depend on another emulator project or emulator core.

## Prohibited

- Source code, binaries, submodules, forks, or vendored directories from PCSX2, ARMSX2, Play!, AetherSX2, or any other emulator project.
- Another emulator's CPU, VU, GS, IOP, BIOS, DMA, JIT, graphics, audio, input, compatibility, or platform implementation.
- Hidden or renamed copies, compatibility shims that load another emulator, and build scripts that fetch or package another emulator.
- Merging another emulator repository or project tree into this repository.

## Allowed dependencies

General-purpose libraries and developer tools may be used when they are not emulator implementations and do not embed an emulator core. Each dependency must be reviewed for license, provenance, platform compatibility, and whether it contains emulator-specific code.

## Hush implementation boundary

Hush must own and implement its PS2 architecture layers: EE/R5900, VU0/VU1, IOP, GS, memory system, DMA/SIF, interrupts, timers, BIOS execution path, audio, controller input, save systems, and iOS integration. The implementation may use public hardware documentation and independently written tests. Research may cite hardware specifications and standards, but must not import another emulator's project or implementation.

## Verification requirements

Before accepting a dependency or code contribution:
1. Inspect the full changed file list, dependency manifests, submodules, binary assets, and build/download scripts.
2. Reject emulator-project source, binaries, copied code, or concealed downloads.
3. Record provenance and license for new dependencies.
4. Keep tests focused on Hush-owned implementations.
5. Never describe Hush as playable based only on CI, synthetic tests, or a simulator build. Real PS2 software must boot and run interactively on supported physical iPhone hardware before claiming playability.

This policy applies to every branch, pull request, release, build workflow, and future contribution in this repository.
