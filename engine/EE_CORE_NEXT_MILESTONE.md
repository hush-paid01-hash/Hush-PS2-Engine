# Hush EE Core: next implementation milestone

This document records the next engineering gates for the original Hush PS2 emulator. It is a work plan, not a claim that the features below are complete or that Hush is playable.

## Current implemented foundation

The repository now has several connected development components:

- A limited EE interpreter with a subset of integer, COP0, memory, and EE-specific instructions. GPR storage has low/high 64-bit halves, but that does not mean complete R5900/MMI semantics.
- An EE memory map for main-RAM aliases, a read-only BIOS ROM window, scratchpad, EE cache-control, and the 2 MiB EE-visible IOP RAM window, shared EE/IOP SIF register latches, and initial BIOS-facing IOP register windows (memory control/RAM_SIZE, DMA control, POST, SPU2 at `0x1F900000`, legacy SPU compatibility, and SIO2). These register latches are partial models, not complete device behavior. TLB behavior and complete reset state are not implemented.
- Canonical sign-extended KSEG instruction fetch is supported, and the bounded BIOS reset-vector runner maps EE timer/INTC devices and samples separate DMAC INT1 into COP0 Cause.IP2 (bit 11), while charging one approximate cycle per attempted instruction. This remains a diagnostic harness, not proof that a real BIOS completes boot.
- Latest real-BIOS trace on the SCPH-30003 V3 UK image executes about 99,000 instructions before an Address Error at PC `0x000014C4` (bad virtual address `0x3084FFFF`). The normal exception vector at `0x80000180` is still uninitialized in guest RAM. BIOS startup therefore remains blocked by incomplete guest exception/IOP/SIF/platform initialization; forcing BEV to stay set was tested only as a diagnostic and is not retained as a hardware fix.
- Four EE timer register blocks mapped only through their implemented `0x2000` window (ending before VIF register space), plus an INTC status/mask register block. Timer event timing is deliberately approximate; external H/V-blank sources and accurate CPU cycle accounting are not modeled.
- A VIF1 register-bank model mapped at `0x10003C00`, covering STAT/FBRST/ERR/MARK, CYCLE/MODE/NUM/MASK/CODE, double-buffer/top registers, and row/column registers. Read/write masks and reset/stall-cancel status behavior have focused tests. Separate bounded 16 KiB VU1 micro-memory and 16 KiB data-memory storage is present. The 16-quadword FIFO, bounded MPG uploads, and a normal-mode EE-RAM-to-VIF1 DMA channel 1 are implemented as partial device slices; source-chain DMA, arbitration, and hardware timing are unsupported. The incremental VIF1 command decoder handles NOP, STCYCL (CL=1/WL=1 only), OFFSET/BASE/ITOP/STMOD/MARK, STMASK/STROW/STCOL payloads, and the supported signed/unsigned S-32/S-16/S-8 and V2/V3/V4 UNPACK subset into VU1 data memory, including split input buffers and TOPS-relative addressing. The BIOS diagnostic bus now maps VIF1 registers, FIFO, and DMA channel to this decoder. Unsupported commands, interrupt-tagged commands, write-masked UNPACK, and remaining UNPACK formats fail closed. VU instruction execution and GIF PATH2/DIRECT are still not implemented.
- A GIF DMA transfer engine with normal and chain modes, connected by a reusable runtime to GIF packet parsing, GS state, local memory, and software framebuffer rendering. Guest CPU writes can program GIF channel 2 through EE DMAC MMIO and reach the GS renderer. D_STAT completion/mask state drives separate DMAC INT1, sampled into COP0 Cause.IP2. Other channels, accurate arbitration/timing, MFIFO, and full device semantics remain unsupported.
- Disc/ELF inspection and a bounded ELF execution subset. Loading an ELF and executing a small instruction subset is not equivalent to booting a retail PS2 game.

The native preview framebuffer is a synthetic GIF test pattern, not output from a running game. The C API correctly reports that game boot is not yet supported. Unit tests, synthetic streams, and CI builds are not evidence of retail-game compatibility.

## Next implementation order

1. **Review the PEXT5 regression fix.** Branch fix/pext5-upper-half-lanes and draft PR #5 correct PEXT5 to read two packed halfwords from each 64-bit half of a 128-bit GPR. Fedora 45 build and tests passed; inspect CI before merge.
2. **Complete VIF1/VU1 execution.** Finish STCYCL cycle/fill behavior, interrupt stalls, FLUSHE/FLUSH/FLUSHA, remaining UNPACK formats (cycle fill/skip and write masks), VU1 micro-instruction execution, and DIRECT/DIRECTHL into GIF PATH2. The current FIFO, MPG, and normal-mode VIF1 DMA slices are synchronous and bounded; they still need hardware-accurate flow control, DMA chain modes, and interrupt/timing behavior.
3. **Expand the EE DMAC beyond GIF channel 2.** Add verified channels and transfer scheduling, then implement arbitration, accurate start/stop semantics, MFIFO, interleave, scratchpad paths, and full D_STAT behavior. Unsupported channels/registers must remain explicit.
4. **Improve interrupt timing and delivery.** The GIF-only DMAC INT1 path now updates COP0 Cause.IP2 independently of INTC INT0. Add accurate cycle timing, interrupt masking/acknowledgement, and tests for CPU exception delivery from both interrupt inputs.
5. **Improve EE timing and interrupt accuracy.** Replace one-cycle-per-instruction approximation with instruction cycle metadata and model timer/DMAC event timing and interrupt masking/acknowledgement accurately enough for boot-critical paths.
6. **Expand EE CPU and COP0 coverage.** Prioritize instructions and exception/ERET behavior required by real BIOS traces. Add regression tests for delay slots, exceptions, TLB-related behavior, and 128-bit MMI operations. Cross-check semantics against the EE manual.
7. **Implement the remaining boot-critical platform path.** Add the IOP/SIF and DMAC channels required by BIOS and game startup, memory-mapped device behavior, and the missing reset/kernel services. Do not substitute host-side shortcuts for guest-visible hardware behavior without documenting them.
8. **Validate real BIOS boot.** Run a legally obtained BIOS image through the guest reset vector, capture a bounded trace, and show concrete firmware progress beyond a reset-vector smoke test.
9. **Reach actual game execution.** Boot a legally obtained PS2 disc image through firmware and guest code, render game-generated GS output, and support input and stable frame updates.
10. **Integrate and verify the iPhone app.** Connect the native engine to a real iOS app target, support BIOS/disc imports through iOS Files, and validate on a physical iPhone. Signing or simulator success alone does not prove gameplay.

## Acceptance gates

- Every hardware milestone has focused regression tests and passing required CI checks.
- No claim of BIOS boot until a real BIOS has demonstrably progressed through firmware execution.
- No claim of playability until a real game reaches interactive gameplay on a physical iPhone build using a genuine BIOS and disc image.
- Synthetic BIOS tests, synthetic GIF streams, CLI demos, and simulator-only builds are useful development checks but do not satisfy the gameplay gate.

## References

- PS2Tek Emotion Engine reference: https://israpps.github.io/ps2tek/PS2/EE/
- PS2Tek VIF register and command reference: https://psi-rockin.github.io/ps2tek/
- PS2 hardware and instruction manuals index: https://github.com/ninjadynamics/PS2Docs
