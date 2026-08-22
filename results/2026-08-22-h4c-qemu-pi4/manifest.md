# H4c qemu-pi4 result — 2026-08-22

## Outcome

The strict H4c smoke oracle passed 100 consecutive fresh QEMU processes.
Every process reached H1 PASS, H2 PASS, and H4c PASS and satisfied every
exact-value and single-occurrence check in `scripts/smoke-qemu.sh`.

In particular, EL1 acknowledged low-priority INTID 42 before high-priority
INTID 43, took exactly one controlled current-EL nested IRQ, and EOI'd the
interrupts in reverse order. `GICH_APR` transitioned from `0x00010000` to
`0x00010010`, back to `0x00010000`, and finally zero while the corresponding
LR0/LR1 states matched the frozen contract. HCR stayed `0x1`; MISR and both
EISR banks stayed zero; no physical IRQ was accepted. No forbidden outcome
was observed. This is a result for the system recorded below, not a general
claim that all GICv2 virtualization behavior is correct.

## Lab image

- Source revision: `db2107b8b506d85ce4644c3554f84f3048094ee5`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 185280 bytes
- ELF SHA-256: `a613ec35f68dfe36f2548baa70a24dfb721b5f65a5b5260dfe8d99382f3d9b52`
- Raw image: `build/gicv2-lab.bin`, 1577276 bytes
- Raw-image SHA-256: `74705404e2e925bcab542784e881f4680b04c141554c9920d3e5707f1d22453b`

The image was clean-built from the recorded revision immediately before the
gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU executable is byte-identical to the binary recorded for the H3,
H4a, and H4b gates. The QEMU worktree's tracked modifications were confined
to `docs/system/arm/raspi-upstream.rst` and
`docs/system/arm/raspi.rst`; its untracked files were `AGENTS.md`,
`docs/system/arm/raspi-gicv2-lab.rst`, and
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

## Linked-image audit

- ELF entry point: `0x80000`
- EL2 vector table: `[0x80800, 0x81000)`, 2 KiB and 2 KiB-aligned
- Monitor end: `0x8e000`, below the guest region at `0x200000`
- EL1 vector table: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ entry: `0x201000`
- Shared nested return restoring `ELR_EL1` and `SPSR_EL1`: `0x2010f8`
- Guest end: `0x20113c`, below its stack boundary at `0x3ff000`

The disassembly audit confirmed that every EL1 vector slot allocates a
64-byte frame and saves `x0`-`x3`, `ELR_EL1`, and `SPSR_EL1` before branching.
The shared IRQ return restores both exception-return registers before ERET.
The linker also enforces the monitor and guest upper bounds with assertions.

## Serial evidence

`serial.log.b64` is a Base64 encoding of the final process's raw, unmodified
5161-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    453dfa397e9f74bf3468d2db50ba1421565c538228d3b22611b49eafd44561ef

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.

After accepting the final PASS marker, the smoke harness terminated the
halted QEMU process with SIGTERM. `qemu.stderr.b64` losslessly preserves its
81-byte termination diagnostic; its decoded SHA-256 is
`7f5dcf8c9b767f6a0035f230d8c4cebf41486b6d835b2133368dde987bc3a917`.
That harness cleanup diagnostic is not a guest reset or failure.
