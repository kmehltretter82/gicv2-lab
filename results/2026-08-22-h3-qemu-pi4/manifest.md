# H3 qemu-pi4 result — 2026-08-22

## Outcome

The strict H3 smoke oracle passed 100 consecutive fresh QEMU processes. Every
process reached H1 PASS, H2 PASS, and H3 PASS and satisfied every exact-value
check in `scripts/smoke-qemu.sh`. No forbidden outcome was observed. This is a
result for the system recorded below, not a general claim that all GICv2
virtualization behavior is correct.

## Lab image

- Source revision: `8890a4d2a41092c1f8feb6dd228682ec54ef4588`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 170208 bytes
- ELF SHA-256: `56a631d0c56cfee373d4b0a13f1e90b66978b1a93ba390c2b5f3b031d5d17660`
- Raw image: `build/gicv2-lab.bin`, 1577068 bytes
- Raw-image SHA-256: `6acd95bf90c21f23458551184bdbc6783fd94b93759504726bd8c85edc452c37`

The image was clean-built immediately before the recorded gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU binary was relinked from the existing
`build-pi4-native-fdt` configuration immediately before this gate. The QEMU
worktree's tracked modifications were confined to two documentation files;
its other untracked files were `AGENTS.md` and documentation. The `-dirty`
version suffix is retained above, and the binary hash is the authoritative
identity of the tested emulator.

## Toolchain and host

- Clang: Homebrew clang 22.1.8
- Linker: Homebrew LLD 22.1.8 at `/opt/homebrew/bin/ld.lld`
- llvm-objcopy: Homebrew LLVM 22.1.8
- Host: macOS 14.8.7 build 23J520, Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Commands

The gate command was:

    make smoke-repeat LLVM_BIN=/opt/homebrew/opt/llvm/bin SMOKE_RUNS=100

Each iteration started a new process equivalent to:

    ../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64 \
        -machine raspi400 \
        -cpu cortex-a72,has_el3=off \
        -kernel build/gicv2-lab.elf \
        -display none \
        -monitor none \
        -serial file:build/smoke-qemu.log \
        -no-reboot

## Serial evidence

`serial.log.b64` is a Base64 encoding of the final process's raw, unmodified
2301-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    2ab16b6a29f7b8fc34d4c33f6ddc8767c0c4c9b3e884c09f8381c4688731f483

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.
