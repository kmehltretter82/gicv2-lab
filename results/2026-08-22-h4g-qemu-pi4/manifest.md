# H4g qemu-pi4 result — 2026-08-22

## Outcome

The frozen strict H4g oracle passed 100 consecutive fresh qemu-pi4
processes. Every process filled all four List Registers with pending INTIDs
40-43 while pending INTID 44 remained solely in the EL2 software queue.

After INTID 40's DIR, LR0 was Invalid and ELRSR0 was `0x1`. EL2 validated
that checkpoint, moved queued encoding `0x1a00002c` into LR0 exactly once,
cleared the queue, and observed ELRSR0 return to zero. The guest then
completed INTIDs 41-44 in strict priority order. Invalid slots accumulated as
ELRSR0 `0x2`, `0x6`, `0xe`, and `0xf`; the final EL2 clear restored four zero
LRs with ELRSR0 still `0xf`.

All 15 Active, priority-drop, and deactivation snapshots matched their exact
LR/APR/RPR/HPPIR values. MISR and EISR remained zero, no physical IRQ or
maintenance event occurred, and there was no extra or missing delivery. The
observed result matches the frozen contract and produced no QEMU bug
candidate.

## Lab image

- Source revision: `7945ed1352c25a2c5146940100c2b024d51ba8e3`
- Source worktree: clean except for this uncommitted result directory during
  the recorded gate
- ELF: `build/gicv2-lab.elf`, 188432 bytes
- ELF SHA-256: `a9ab315a0969484d8deb04f0e5cf7ba6a3b95aa7ceff1f4edb8f2010d1a3cc22`
- Raw image: `build/gicv2-lab.bin`, 1577192 bytes
- Raw-image SHA-256: `cd111ff746f9e0d5019c38a9993d601d33931c2d70ffb635d194fbdcdb77b13b`

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
- Host: macOS 14.8.7, Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Commands

    make clean
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin
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
- Stage-2 tables: `0x87000`, `0x88000`, `0x89000`, and `0x8a000`
- Monitor end: `0x8f000`, below the guest region at `0x200000`
- EL1 vectors: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ dispatcher: `0x201000`
- Shared IRQ return: `0x2010ac`
- Delivery counter: `0x2010e4`
- Guest end: `0x2010e8`, below its stack boundary at `0x3ff000`

The guest contains a single bounded interrupt-handler loop, one static IAR
read, one EOIR write, and one DIR write. Both the main loop and handler reject
a count outside `[0, 5)`, while the EL2 monitor independently requires five
ordered executions of each phase. The handler retains the raw IAR value
through both writes and restores `ELR_EL1` and `SPSR_EL1` before ERET. Every
other EL1 vector is fatal.

The stage-2 mappings still expose only the two adjacent Device-nGnRE, RW, XN
GICV pages at `0xff846000` and `0xff847000`. EL1 cannot access GICD, GICC,
GICH, PL011, or surrounding MMIO.

## Serial evidence

`serial.log.b64` losslessly preserves the final process's raw 16053-byte
serial trace, including CRLF line endings. Its decoded SHA-256 is:

    b9a5c18e388b6f5cf74be15b6f0dfffe92ebf9908947c5698566041c3522ce73

`qemu.stderr.b64` preserves the 81-byte harness termination diagnostic with
decoded SHA-256:

    a46c1407dd2f4c9ed82286f3caca6bd1815e03c4d2cb10fdc65f89bffe567de0

The repetition loop replaces the live logs each iteration. The strict oracle
evaluated the complete trace for all 100 processes; the preserved trace is
the final passing iteration. The SIGTERM diagnostic is expected harness
cleanup after PASS, not a guest reset.
