# H4i qemu-pi4 result — 2026-08-22

## Outcome

The frozen strict H4i oracle passed 100 consecutive fresh qemu-pi4
processes. Each process started with an empty virtual interface, delivered
lower-priority virtual INTID 50, and paused after the guest acknowledged it.
EL2 then added pending higher-priority INTID 51 and saved HCR, VMCR, APR, and
all four implemented LRs.

Every run proved that EL2 disabled HCR first, installed a distinct quiescent
context, restored VMCR/APR/LR0-LR3 while HCR remained zero, and enabled the
saved HCR last. The restored complete snapshot exactly matched the saved
active-plus-pending snapshot. After returning to EL1, INTID 51 preempted the
INTID 50 handler; both interrupts completed in reverse priority order through
separate EOIR and DIR writes.

MISR and EISR remained zero, all PPIs stayed disabled, and no physical IRQ,
maintenance event, extra or missing delivery, unexpected exception, reset,
hang, or early exit occurred. The observed result matches the frozen
contract and produced no QEMU bug candidate.

During pre-freeze development, a literal-zero VMCR readback expectation for
the quiescent context failed. A zero VMCR write correctly read back as
`0x004c0000`: virtual BPR and ABPR were clamped to their minimum values 2 and
3 for five implemented priority bits. The frozen oracle distinguishes the
zero write from that canonical readback. This was a lab expectation error,
not a QEMU divergence.

## Lab image

- Source revision: `0e4374701ff73860a300e20ba76c75d45cf14d04`
- Source worktree: clean during the build and 100-process gate
- ELF: `build/gicv2-lab.elf`, 200368 bytes
- ELF SHA-256: `0bef6082deb828d5b423bc5e081965f05bfa601eaced8dc853bee554541ddec2`
- Raw image: `build/gicv2-lab.bin`, 1577472 bytes
- Raw-image SHA-256: `9241a235f93402cebddbe6bced4f4fdf121b0f05bb9ba6913db66008a63e73cb`

The image was clean-built from the recorded revision immediately before the
single strict run and repetition gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source revision: `8460833e53a91458fd3ba63ff19fb6c4932e8bb4`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-18-g8460833e53-dirty)`
- QEMU binary SHA-256: `bf3e2f36b451af4225d3fd71eabe8981dd8ea05558977825a355690f21426c16`

The QEMU worktree retained its local Raspberry Pi documentation changes and
untracked instruction/lab documents. The `-dirty` suffix is recorded, and the
binary hash is the authoritative emulator identity.

## Toolchain and host

- Clang: Homebrew clang 22.1.8
- Linker: Homebrew LLD 22.1.8
- llvm-objcopy/llvm-objdump: Homebrew LLVM 22.1.8
- Host: macOS 14.8.7 (23J520), Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Commands

    make clean
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin -j4 all
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin SMOKE_RUNS=100 smoke-repeat

Each iteration started a new process equivalent to:

    ../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64 \
        -machine raspi400 \
        -cpu cortex-a72,has_el3=off \
        -kernel build/gicv2-lab.elf \
        -display none \
        -monitor none \
        -serial file:build/smoke-qemu.log \
        -no-reboot

## Stage-2 and linked-image audit

- ELF entry point: `0x80000`
- EL2 vectors: `[0x80800, 0x81000)`, 2 KiB and 2 KiB-aligned
- H4i context save/restore routine: `0x83200`
- Stage-2 L1 table: `0x88000`
- Stage-2 guest L2 table: `0x89000`
- Stage-2 MMIO L2 table: `0x8a000`
- Stage-2 GICV L3 table: `0x8b000`
- Monitor stack: `[0x8c000, 0x90000)`
- Monitor end: `0x90000`, below the guest region at `0x200000`
- EL1 vectors: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ dispatcher: `0x201000`
- Lower-priority handler: `0x201018`
- Higher-priority nested handler: `0x20110c`
- Shared IRQ return: `0x2011bc`
- Handler-depth word: `0x2011f4`
- Lower/high completion words: `0x2011f8` and `0x2011fc`
- Guest end: `0x201200`, below its stack boundary at `0x3ff000`

The active guest path contains exactly two static GICV IAR reads: one in each
handler. Each handler retains the low 32-bit raw IAR through one EOIR write
and one DIR write. The lower handler unmasks IRQs only after the restored
context is active and handler depth is one; the nested handler requires that
depth and restores it before returning. Every other EL1 vector is fatal.

The stage-2 mappings still expose only the two adjacent Device-nGnRE, RW, XN
GICV pages at `0xff846000` and `0xff847000`. EL1 cannot access GICD, GICC,
GICH, PL011, or surrounding MMIO. No physical interrupt is part of H4i.

## Serial evidence

`serial.log.b64` losslessly preserves the final process's raw 11695-byte
serial trace, including CRLF line endings. Its decoded SHA-256 is:

    ecf96261e7a9e7ed5c553bdbed0d4f7f56b78637a89db12df9e8bd166fabf457

`qemu.stderr.b64` preserves the 81-byte harness termination diagnostic with
decoded SHA-256:

    d963195eeec0ff92bea7ee46558024f90e2047901151f580abf8fd92f5d8a580

The repetition loop replaces the live logs each iteration. The strict oracle
evaluated the complete trace for all 100 processes; the preserved trace is
the final passing iteration. The SIGTERM diagnostic is expected harness
cleanup after PASS, not a guest reset.
