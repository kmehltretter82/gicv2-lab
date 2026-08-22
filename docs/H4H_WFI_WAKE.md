# H4h trapped WFI and timer-driven virtual wake

## Architectural contract

H4h isolates a guest wait and wake sequence. It proves that EL1 reaches one
specific `WFI`, re-executes that instruction without trapping, and resumes
only after one EL2 physical timer interrupt has injected and completed one
virtual interrupt.

The initial state is an enabled but empty virtual CPU interface:

- `GICH_HCR=0x1`, `GICH_VMCR=0xf85c0201`, and `GICH_APR=0`;
- all four LRs are zero, `GICH_ELRSR0=0xf`, and MISR/EISR are zero;
- guest-visible HPPIR is the spurious INTID 1023; and
- every PPI is disabled except non-secure EL2 physical timer PPI 26, which is
  level-sensitive at priority `0x20`.

The required sequence is:

1. EL1 reports the empty interface while IRQs are masked. EL2 sets
   `HCR_EL2.TWI`, changing HCR from `0x80000011` to `0x80002011`.
2. EL1 unmasks IRQs and executes the globally named
   `guest_wfi_instruction`. The lower-EL synchronous exception must have WFx
   syndrome `ESR_EL2=0x07e00000`, select WFI rather than WFE, preserve EL1h
   with IRQ unmasked in SPSR, and leave ELR pointing at that exact symbol.
3. EL2 clears TWI without changing ELR, then arms `CNTHP_EL2` for
   `CNTFRQ_EL0 / 20` ticks. Returning from the exception therefore
   re-executes the same WFI instead of skipping it.
4. The timer must enter EL2 through lower-AArch64 IRQ vector slot 9. The IRQ's
   ELR must be the globally named instruction immediately after WFI, and its
   SPSR must still describe EL1h with IRQ unmasked. Physical IAR must identify
   INTID 26, `CNTHP_CTL_EL2` must contain ENABLE and ISTATUS with IMASK clear,
   and the observed counter delta must be at least the requested delay. EL2
   disables the timer, installs pending virtual INTID 48 at priority `0x40`
   in LR0, and EOIs physical INTID 26.
5. EL1 wakes, acknowledges INTID 48, and completes split EOI. The exact LR0
   sequence is Pending `0x14000030`, Active `0x24000030`, still Active after
   EOIR, Invalid `0x04000030` after DIR, then zero after EL2 clears it. APR is
   `0x100` only at the Active checkpoint.
6. The handler increments one counter. The instruction after WFI may report
   completion only after that counter is exactly one, after which EL2 accepts
   EXIT.

This ordering rejects two important false passes. If WFI behaves like a NOP,
the post-WFI count check executes before the timer and fails. If the timer is
taken before the re-executed WFI, its exact IRQ ELR check fails. A missing
trap, skipped instruction, duplicate wake, unrelated physical IRQ,
maintenance event, unexpected exception, reset, or hang is also forbidden.

The delay is a deterministic lab policy, not a timing-fidelity assertion.
The monitor checks only the architectural lower bound (`elapsed >= delay`);
it deliberately makes no upper-bound or host-latency claim.

The WFx trap and generic-timer rules are described by the
[Arm Architecture Reference Manual for A-profile architecture](https://developer.arm.com/documentation/ddi0487/latest/).
The physical and virtual interrupt-interface rules are described by the
[Arm Generic Interrupt Controller Architecture Specification, version 2](https://developer.arm.com/documentation/ihi0048/latest/).

## Exit gate

Freeze the implementation first, then require 100 consecutive fresh
qemu-pi4 processes accepted by `scripts/smoke-qemu.sh`. Preserve the exact
source revision, image and emulator hashes, toolchain, host, command line,
linked-image audit, and lossless final trace under `results/` before marking
H4h complete.

## Recorded QEMU result

The frozen implementation at `da2fa922c71acf0c7f0644e2bc21cce8db6156e4`
passed 100 consecutive fresh qemu-pi4 processes on 2026-08-22. The exact
image, emulator, environment, linked-image audit, and lossless final trace are
recorded in `results/2026-08-22-h4h-qemu-pi4/manifest.md`.

The observed result matched this contract. H4h produced no divergent
behavior and no QEMU bug candidate.
