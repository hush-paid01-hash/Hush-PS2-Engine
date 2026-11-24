# Hush Original PS2 Engine — Research Notes

**Status:** implementation and hardware-validation research
**Project rule:** Hush is an independent PS2 emulator. Do not integrate, vendor, wrap, bundle, or build another emulator project or core.

## Scope

- Develop Hush-owned implementations of the PlayStation 2 EE/R5900, VU0/VU1, IOP, GS, memory, DMA/SIF, interrupts, timers, BIOS execution, audio, and iOS integration.
- Use primary hardware documentation and independently written tests.
- Research iOS execution, JIT restrictions, signing, installation, and native build constraints.
- Validate compatibility through real BIOS and game execution on physical iPhone hardware.

## Explicit exclusions

- Do not use PCSX2, ARMSX2, Play!, AetherSX2, or another emulator as a source tree, core, binary, submodule, build input, or runtime dependency.
- Do not copy or adapt another emulator's implementation.
- Do not create CI workflows that check out or package another emulator.
- General-purpose developer tools and libraries may be considered only after provenance and license review and only when they do not embed an emulator core.

## Focus areas

| # | Area | Status |
|---|---|---|
| 1 | EE/R5900 instruction coverage, exceptions, and BIOS execution | In progress |
| 2 | VU0/VU1 execution and VIF pathways | In progress |
| 3 | GS rendering, GIF, and DMA correctness | In progress |
| 4 | IOP, SIF, audio, and peripheral behavior | Planned |
| 5 | iOS ARM64 execution, JIT constraints, and native app integration | Planned |
| 6 | Real BIOS and game validation on physical iPhone | Not yet demonstrated |

## Acceptance standard

A passing unit test, synthetic frame, BIOS trace, simulator build, or successful CI run is not proof of playability. A playability claim requires an actual PS2 title to boot and run interactively on supported physical iPhone hardware.

See [ORIGINAL_ENGINE_POLICY.md](../docs/ORIGINAL_ENGINE_POLICY.md) for the repository-wide contribution and dependency rules.
