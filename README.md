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

H1 is the current milestone. It deliberately stays QEMU-only:

- reset at non-secure EL2;
- PL011 serial output;
- an EL2 exception-vector table;
- a capability dump, including GICH_VTR;
- one deliberate synchronous exception; and
- a deterministic H1 PASS serial marker.

The first real Pi 400 boot is scheduled only after H1 through H3 pass under
QEMU. See ROADMAP.md.

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

The test loads the ELF directly at 0x80000 and uses
-cpu cortex-a72,has_el3=off. That prevents QEMU's optional synthetic EL3 from
obscuring the non-secure EL2 environment the Pi 400 exposes to this lab. It
never writes a physical boot medium.

## Repository boundaries

The lab is deliberately separate from qemu-pi4: it must be able to test the
fork, unmodified QEMU, KVM, and hardware without becoming part of any one
system under test. It does not use a Git submodule; result manifests will pin
the tested revisions instead.

Source files are GPL-2.0-or-later. See LICENSE and docs/PROVENANCE.md.
