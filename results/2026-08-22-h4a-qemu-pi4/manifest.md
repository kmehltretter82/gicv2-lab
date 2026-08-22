# H4a qemu-pi4 result — 2026-08-22

## Outcome

The strict H4a smoke oracle passed 100 consecutive fresh QEMU processes.
Every process reached H1 PASS, H2 PASS, and H4a PASS and satisfied every
exact-value check in `scripts/smoke-qemu.sh`, including the initial, pending,
active, post-EOI, and cleared LR0/APR snapshots. No forbidden outcome was
observed. This is a result for the system recorded below, not a general claim
that all GICv2 virtualization behavior is correct.

## Lab image

- Source revision: `c965ac10b0f539ff54ff7903650a70c354f10e69`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 176688 bytes
- ELF SHA-256: `8fc2bd120d367d6343bd9bc9fc255d9081137b3f449feb282c6aaf590dc93ea0`
- Raw image: `build/gicv2-lab.bin`, 1577080 bytes
- Raw-image SHA-256: `cddecd66d67c69ba076a1bcbe048bc805ef54a9ffe9d1bf0679e3113db3fa34a`

The image was clean-built from the recorded revision immediately before the
gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU executable is byte-identical to the relinked binary recorded for the
H3 gate. The QEMU worktree's tracked modifications were confined to
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

The clean build and gate commands were:

    make clean
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke-repeat

`SMOKE_RUNS` was not overridden, so the Makefile default of 100 applied. Each
iteration started a new process equivalent to:

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
- Monitor end: `0x8d000`, below the guest region at `0x200000`
- EL1 vector table: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest end: `0x201078`, below its stack boundary at `0x3ff000`

The linker also enforces the monitor and guest upper bounds with assertions.

## Serial evidence

`serial.log.b64` is a Base64 encoding of the final process's raw, unmodified
3386-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    8aeb3fda6cf4101c109adb53827be155d269556b54b5453bd38f3aaf89fd7783

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.
