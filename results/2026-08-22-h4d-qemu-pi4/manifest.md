# H4d qemu-pi4 result — 2026-08-22

## Outcome

The strict H4d smoke oracle passed 100 consecutive fresh QEMU processes.
Every process reached H1 PASS, H2 PASS, and H4d PASS and satisfied every
exact-value and single-occurrence check in `scripts/smoke-qemu.sh`.

In particular, EL1 acknowledged software virtual INTID 42 with
`GICV_RPR=0x80`. Its EOIR write cleared GICH_APR and changed RPR to `0xff`
while LR0 remained active at `0x2800002a`. Only the subsequent DIR write
changed LR0 to the invalid encoding `0x0800002a`; EL2 then cleared it. HCR
stayed `0x1`, MISR and both EISR banks stayed zero, EOICount stayed zero, LR1
stayed empty, and no physical IRQ was accepted. No forbidden outcome was
observed. This is a result for the system recorded below, not a general claim
that all GICv2 virtualization behavior is correct.

The observed result matches the frozen architectural contract. H4d therefore
produced no divergent behavior and no QEMU upstream bug candidate.

## Lab image

- Source revision: `f1d8ff93720ebdd49f5140d59ebd822e68865e6e`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 180528 bytes
- ELF SHA-256: `841278fb5fec22d072f3097eb3f473ee4b91fa8e3e8a0e7ccff47abca6867beb`
- Raw image: `build/gicv2-lab.bin`, 1577144 bytes
- Raw-image SHA-256: `c64602b5e05e2e3312baf99703587d7915faaaaaaa68bff04b5986f8c1a2ba47`

The image was clean-built from the recorded revision immediately before the
gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU executable is byte-identical to the binary recorded for the H3
through H4c gates. The QEMU worktree's tracked modifications were confined to
`docs/system/arm/raspi-upstream.rst` and `docs/system/arm/raspi.rst`; its
untracked files were `AGENTS.md`, `docs/system/arm/raspi-gicv2-lab.rst`, and
`docs/system/arm/raspi-upstream-criteria.rst`. The `-dirty` version suffix is
retained above, and the binary hash is the authoritative identity of the
tested emulator.

## Toolchain and host

- Clang: Homebrew clang 22.1.8
- Linker: Homebrew LLD 22.1.8 at `/opt/homebrew/bin/ld.lld`
- llvm-objcopy: Homebrew LLVM 22.1.8
- Host: macOS 14.8.7 build 23J520, Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Commands

The implementation was frozen first. The clean build and gate commands were:

    make clean
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke-repeat

`SMOKE_RUNS` was not overridden, so the Makefile default of 100 applied. Each
gate iteration started a new process equivalent to:

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
- EL2 vector table: `[0x80800, 0x81000)`, 2 KiB and 2 KiB-aligned
- Monitor end: `0x8d000`, below the guest region at `0x200000`
- EL1 vector table: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ entry: `0x201000`
- Shared IRQ return restoring `ELR_EL1` and `SPSR_EL1`: `0x20107c`
- Guest completion word: `0x2010b4`
- Guest end: `0x2010b8`, below its stack boundary at `0x3ff000`

The disassembly audit confirmed that every EL1 vector slot allocates a
64-byte frame and saves `x0`-`x3`, `ELR_EL1`, and `SPSR_EL1` before branching.
The split-EOI handler reads IAR at GICV offset `0x00c` and RPR at `0x014`,
writes EOIR at `0x010`, validates the priority-drop checkpoint through HVC
`0x105`, then writes DIR at `0x1000` and validates deactivation through HVC
`0x107`. The shared return restores both exception-return registers before
ERET. The linker enforces the monitor and guest upper bounds with assertions.

The guest-visible GICV mappings were exactly the two adjacent Device-nGnRE,
RW, XN pages required for the architectural 8 KiB CPU-interface block:

- `0xff846000`: descriptor `0x00400000ff8467c7`
- `0xff847000`: descriptor `0x00400000ff8477c7`

The stage-2 table is zero-filled before those two entries are installed, so
GICD, GICC, GICH, PL011, and the surrounding MMIO pages remain unavailable to
EL1.

## Serial evidence

`serial.log.b64` is a Base64 encoding of the final process's raw, unmodified
4359-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    7ade32106f7e2053a15127a8b88a7a742a7fb281cfba69074fc5ff1ec75e9f24

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.

After accepting the final PASS marker, the smoke harness terminated the
halted QEMU process with SIGTERM. `qemu.stderr.b64` losslessly preserves its
79-byte termination diagnostic; its decoded SHA-256 is
`151f0a296215535df79e2c51cd6f0eda6714991eeeef7da5c2d1580afa64fb45`.
That harness cleanup diagnostic is not a guest reset or failure.
