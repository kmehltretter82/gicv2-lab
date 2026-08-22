# H4b qemu-pi4 result — 2026-08-22

## Outcome

The strict H4b smoke oracle passed 100 consecutive fresh QEMU processes.
Every process reached H1 PASS, H2 PASS, and H4b PASS and satisfied every
exact-value check in `scripts/smoke-qemu.sh`. In particular, two valid LRs
produced no maintenance condition, the primary EOI left one masked pending LR
and asserted only `GICH_MISR.U`, both EISR banks remained zero, and EL2
restored the interface to its initial quiescent state. No forbidden outcome
was observed. This is a result for the system recorded below, not a general
claim that all GICv2 virtualization behavior is correct.

## Lab image

- Source revision: `5f3426ae5f004d153964fc9a53ee23eae148d486`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 177656 bytes
- ELF SHA-256: `a24a3a0fe1a690521641eabcdd98a7330a992df086f52a8eabad5698855a195f`
- Raw image: `build/gicv2-lab.bin`, 1577080 bytes
- Raw-image SHA-256: `5947f2715cc4366a4885f77b9b0e14e06517ab8c3157ff338a4ed0d4d827c63b`

The image was clean-built from the recorded revision immediately before the
gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU executable is byte-identical to the relinked binary recorded for the
H3 and H4a gates. The QEMU worktree's tracked modifications were
confined to `docs/system/arm/raspi-upstream.rst` and
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
3599-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    2374aaa276ea587f0d67ba295a6914138c5cd59a3d13d6f24f200d49a6ad9c67

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.
