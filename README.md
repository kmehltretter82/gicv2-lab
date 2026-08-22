# gicv2-lab

Deterministic AArch64 EL2 lab for differential GICv2 virtualization testing on
Raspberry Pi 4/400, QEMU, and Linux KVM.

gicv2-lab is a small bare-metal monitor and a collection of tiny EL1 test
payloads. It is not a production hypervisor. Its purpose is to make a virtual
interrupt transition observable, replayable, and comparable across:

1. the qemu-pi4 fork and unmodified QEMU under TCG;
2. a Pi 400's physical GIC-400 virtualization interface; and
3. Linux KVM's KVM_DEV_TYPE_ARM_VGIC_V2 interface.

## Status

H4h is complete under qemu-pi4, and the H4i implementation passes its strict
single-process pre-gate run. The QEMU-only lab now has:

- the H1 EL2 monitor and H2 one-vCPU EL1 guest with a validated, resumable
  stage-2 translation fault;
- a stage-2 mapping for exactly the 8 KiB GICV virtual CPU-interface block;
- dynamic List Register discovery and deterministic GICD/GICC/GICH setup;
- staged injection of software virtual INTIDs 42 and 43 through LR0 and LR1;
- EL1 acknowledgement and EOI through GICV, including one controlled nested
  current-EL IRQ;
- EL2 capture, validation, clearing, and physical EOI of maintenance PPI 25;
- exact empty, pending, active, post-EOI, and cleared LR snapshots, including
  their corresponding GICH_APR transitions;
- an isolated underflow-maintenance transition with two valid software LRs,
  a masked pending reserve, and no EOI/EISR cause;
- BPR 2 priority preemption from INTID 42 at priority `0x80` to INTID 43 at
  priority `0x20`, with exact both-active and reverse-completion checkpoints;
- split EOI mode for one software virtual interrupt, with guest-visible RPR
  and exact LR/APR state proving that EOIR drops priority while DIR performs
  the later deactivation; and
- one controlled Active-to-Pending+Active re-pend, with HPPIR eligibility,
  Pending exposure at the first DIR, exactly one second delivery, and final
  deactivation all checked against exact LR/APR state; and
- two software virtual SGIs with distinct source CPUID fields carried through
  raw HPPIR, IAR, split EOI, and deactivation values; and
- full four-LR exhaustion, one fifth interrupt held in an EL2 software queue,
  and a single validated empty-slot refill followed by five ordered
  completions; and
- an exact `HCR_EL2.TWI` trap and same-PC re-execution of guest WFI, followed
  by one `CNTHP_EL2` PPI and one timer-injected virtual wake interrupt; and
- a paused active-plus-pending virtual-interface context saved across HCR,
  VMCR, APR, and all four LRs, replaced by a distinct quiescent context, and
  restored with HCR enabled last before nested delivery and split completion.

The strict H4h exit gate passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22. Its exact revision, hashes, environment, linked-image audit, and
final raw trace are preserved in `results/2026-08-22-h4h-qemu-pi4/`. H4h
matched its frozen contract and produced no QEMU bug candidate. H4f's earlier
fork candidate and its separate pre-fix evidence remain recorded under the
H4f result directory.

The H4i implementation and strict oracle still require their frozen
100-process exit gate and evidence manifest. No physical boot has been
performed. Hardware execution remains a separate, explicitly approved H6
step. See ROADMAP.md.

## Build

The project uses LLVM's freestanding AArch64 tools. On this Mac:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin

On a system where LLVM tools are on PATH:

    make

This produces:

- build/gicv2-lab.elf — direct-QEMU image;
- build/gicv2-lab.bin — raw image retained for later firmware boot work;
- build/gicv2-lab.map and build/gicv2-lab.dis — audit artifacts.

## QEMU smoke test

The default test uses the local sibling qemu-rpi4 checkout:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke

Override QEMU to test another build:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin \
      QEMU=/path/to/qemu-system-aarch64 smoke

Run the H4i exit-gate loop (100 consecutive boots by default) with:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke-repeat

Set `SMOKE_RUNS=N` to select a different count.

The test loads the ELF directly at 0x80000 and uses
-cpu cortex-a72,has_el3=off. That prevents QEMU's optional synthetic EL3 from
obscuring the non-secure EL2 environment the Pi 400 exposes to this lab. It
checks the H1 and H2 descriptors and exceptions, the virtual-interrupt path,
every H4i HCR/VMCR/LR/APR/MISR/EISR/ELRSR snapshot, the exact saved and
restored writable context, the nested guest-visible IAR/RPR/HPPIR sequence,
and the final PASS marker. It never writes a physical boot medium.

The H2 architectural contract and fixed memory map are documented in
docs/H2_STAGE2.md. The virtual-interrupt contract is in docs/H3_GICV2.md, the
LR lifecycle is in docs/H4A_LR_LIFECYCLE.md, underflow maintenance is in
docs/H4B_UNDERFLOW.md, priority preemption is in
docs/H4C_PRIORITY_PREEMPTION.md, split EOI is in docs/H4D_SPLIT_EOI.md, and
Active+Pending redelivery is in docs/H4E_ACTIVE_PENDING.md. Multi-source SGIs
are in docs/H4F_MULTI_SOURCE_SGI.md, LR exhaustion/refill is in
docs/H4G_LR_REFILL.md, WFI wake is in docs/H4H_WFI_WAKE.md, paused
virtual-interface context save/restore is in
docs/H4I_CONTEXT_SAVE_RESTORE.md, and the guest call interface is in
docs/HVC_ABI.md.

## Repository boundaries

The lab is deliberately separate from qemu-pi4: it must be able to test the
fork, unmodified QEMU, KVM, and hardware without becoming part of any one
system under test. It does not use a Git submodule; result manifests will pin
the tested revisions instead.

Source files are GPL-2.0-or-later. See LICENSE and docs/PROVENANCE.md.
