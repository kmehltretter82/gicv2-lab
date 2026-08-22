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

H3 is complete under qemu-pi4. It deliberately stays QEMU-only and now has:

- the H1 EL2 monitor and H2 one-vCPU EL1 guest with a validated, resumable
  stage-2 translation fault;
- a stage-2 mapping for only the 4 KiB GICV virtual CPU-interface page;
- dynamic List Register discovery and deterministic GICD/GICC/GICH setup;
- injection of software virtual INTID 42 through LR0;
- EL1 acknowledgement and EOI through GICV; and
- EL2 capture, validation, clearing, and physical EOI of maintenance PPI 25.

The strict H3 exit gate passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22. The tested revision, hashes, environment, and final raw trace are
preserved in `results/2026-08-22-h3-qemu-pi4/`.

The QEMU prerequisite for the first real Pi 400 boot is now satisfied, but no
physical boot has been performed. Hardware execution remains a separate,
explicitly approved H6 step. H4's deterministic state-machine scenarios are
the next QEMU-only development milestone. See ROADMAP.md.

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

Run the H3 exit-gate loop (100 consecutive boots by default) with:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke-repeat

Set `SMOKE_RUNS=N` to select a different count.

The test loads the ELF directly at 0x80000 and uses
-cpu cortex-a72,has_el3=off. That prevents QEMU's optional synthetic EL3 from
obscuring the non-secure EL2 environment the Pi 400 exposes to this lab. It
checks the H1, H2, and H3 descriptors, exception traces, List Register state,
maintenance event, and final PASS marker. It never writes a physical boot
medium.

The H2 architectural contract and fixed memory map are documented in
docs/H2_STAGE2.md. The virtual-interrupt contract is in docs/H3_GICV2.md, and
the guest call interface is in docs/HVC_ABI.md.

## Repository boundaries

The lab is deliberately separate from qemu-pi4: it must be able to test the
fork, unmodified QEMU, KVM, and hardware without becoming part of any one
system under test. It does not use a Git submodule; result manifests will pin
the tested revisions instead.

Source files are GPL-2.0-or-later. See LICENSE and docs/PROVENANCE.md.
