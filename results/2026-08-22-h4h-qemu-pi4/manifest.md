# H4h qemu-pi4 result — 2026-08-22

## Outcome

The frozen strict H4h oracle passed 100 consecutive fresh qemu-pi4
processes. Each process began with an empty virtual interface, trapped the
guest's exact WFI through HCR_EL2.TWI, cleared TWI without advancing ELR, and
re-executed that WFI.

The non-secure EL2 physical timer then entered through lower-AArch64 IRQ vector
slot 9 with physical INTID 26. Its return ELR was exactly the instruction
after WFI, and SPSR showed EL1h with IRQ unmasked. EL2 injected one pending
virtual INTID 48. The guest observed the exact Pending, Active, priority-drop,
Invalid, and cleared LR0/APR states, completed the handler once, and resumed
after WFI with count one.

MISR and EISR remained zero. No unrelated physical IRQ, maintenance event,
extra/missing wake, unexpected exception, reset, hang, or early continuation
occurred. The observed result matches the frozen contract and produced no
QEMU bug candidate.

## Lab image

- Source revision: `da2fa922c71acf0c7f0644e2bc21cce8db6156e4`
- Source worktree: clean except for this uncommitted result directory during
  evidence recording
- ELF: `build/gicv2-lab.elf`, 189384 bytes
- ELF SHA-256: `62fc7cba11d3cf5b4eff51ffde087898d93549e72f5baf6f95c6ecc563befe97`
- Raw image: `build/gicv2-lab.bin`, 1577180 bytes
- Raw-image SHA-256: `4d86d61e7d15d72a8c7a2d47c38970c087df7af7b916017fdb476376368b2e0b`

The image was clean-built from the recorded revision immediately before the
gate.

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
- Stage-2 L1 table: `0x86000`
- Stage-2 guest L2 table: `0x87000`
- Stage-2 MMIO L2 table: `0x88000`
- Stage-2 GICV L3 table: `0x89000`
- Monitor end: `0x8e000`, below the guest region at `0x200000`
- Guest WFI: `0x200074`
- Instruction immediately after WFI: `0x200078`
- EL1 vectors: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ dispatcher: `0x201000`
- Shared IRQ return: `0x2010a0`
- Delivery counter: `0x2010d8`
- Guest end: `0x2010dc`, below its stack boundary at `0x3ff000`

The active guest path contains exactly one WFI. The timer IRQ requires ELR
`0x200078`, proving the asynchronous exception was taken from that wait. The
guest has one static GICV IAR read, one EOIR write, and one DIR write; it
retains raw IAR through both completion writes. It rejects a second delivery
before IAR and requires exactly one completed handler after WFI. Every other
EL1 vector is fatal.

The stage-2 mappings still expose only the two adjacent Device-nGnRE, RW, XN
GICV pages at `0xff846000` and `0xff847000`. EL1 cannot access GICD, GICC,
GICH, PL011, the physical timer controls, or surrounding MMIO.

## Serial evidence

`serial.log.b64` losslessly preserves the final process's raw 7534-byte
serial trace, including CRLF line endings. Its decoded SHA-256 is:

    b9a18038f36f16614085bef295c7825159cd3a7426b3d4192ea0536580fb8ec3

`qemu.stderr.b64` preserves the 81-byte harness termination diagnostic with
decoded SHA-256:

    740bbf2ac5c3547cbf1660225b06e0e77d7f86ef260724f31d2a4ac324db0a06

The repetition loop replaces the live logs each iteration. The strict oracle
evaluated the complete trace for all 100 processes; the preserved trace is
the final passing iteration. The SIGTERM diagnostic is expected harness
cleanup after PASS, not a guest reset.
