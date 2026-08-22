# H4f qemu-pi4 result — 2026-08-22

## Outcome

The frozen H4f scenario exposed one deterministic qemu-pi4 divergence and
then passed 100 consecutive fresh processes with a focused fork fix.

Before the fix, both software virtual SGIs were present with the required
List Register encodings (`LR0=0x12000405`, `LR1=0x18000c06`), but the first
guest `GICV_HPPIR` read returned `0x0005` instead of the required raw value
`0x0405`. The strict oracle stopped immediately with:

    GuestHPPIR_both_pending_observed=0x0000000000000005
    [gicv2-lab] H4f FAIL: invalid initial SGI HPPIR report

The fork fix makes `GICV_HPPIR` include the CPUID field of a pending
software-originated virtual SGI, matching the existing `GICV_IAR` behavior.
With that change, both CPUID values survived HPPIR, acknowledgement, split
priority drop, and deactivation. Every exact LR/APR/RPR/HPPIR checkpoint
matched the contract, and no maintenance interrupt or physical IRQ occurred.

This is an E1 fork defect candidate, not an upstream-ready report. It has not
been reproduced on unmodified current upstream master, searched against
current upstream reports, or manually validated by the user. The lab and fix
were produced with Codex assistance; this QEMU commit must not be proposed as
an upstream patch. No issue, email, comment, or patch was sent externally.

## Lab image

- Source revision: `720149dbd00dc4eecb390125fd54afe5ce4fcc0c`
- Source worktree: clean except for this uncommitted result directory during
  the recorded gate
- ELF: `build/gicv2-lab.elf`, 190000 bytes
- ELF SHA-256: `b2c92fdf0c28d2eaaed2604fb966d02981c217fcd28b12c268a44dbb9890e633`
- Raw image: `build/gicv2-lab.bin`, 1577344 bytes
- Raw-image SHA-256: `ae1cd26286646d57c2e7c924877a967b175731055e76c0710cb5183bbd464b40`

The image was clean-built from the recorded revision immediately before the
passing gate. The same image hashes were used for the pre-fix observation.

## Systems under test

Common configuration:

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`

Pre-fix observation:

- QEMU source revision: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

Passing gate:

- QEMU source revision: `8460833e53a91458fd3ba63ff19fb6c4932e8bb4`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-18-g8460833e53-dirty)`
- QEMU binary: `build-pi4-native-fdt/qemu-system-aarch64`, 15687744 bytes
- QEMU binary SHA-256: `bf3e2f36b451af4225d3fd71eabe8981dd8ea05558977825a355690f21426c16`
- Focused change: `hw/intc/arm_gic: include virtual SGI source in HPPIR`

The QEMU worktree's unrelated tracked modifications were confined to the two
Raspberry Pi documentation files shown by `git status`; its untracked files
were the local repository instructions and Raspberry Pi lab/criteria
documents. The `-dirty` suffix is retained, and the executable hashes are the
authoritative emulator identities.

## Toolchain and host

- Clang: Homebrew clang 22.1.8
- Linker: Homebrew LLD 22.1.8
- llvm-objcopy/llvm-objdump: Homebrew LLVM 22.1.8
- Host: macOS 14.8.7, Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Commands

The frozen image first reproduced the forbidden outcome against the pre-fix
binary with the equivalent command:

    QEMU=../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64 \
        scripts/smoke-qemu.sh build/gicv2-lab.bin

After committing and rebuilding the QEMU fix, the recorded build and gate
were:

    make clean
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin SMOKE_RUNS=100 smoke-repeat

Every gate iteration launched a new process equivalent to:

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
- Stage-2 tables: `0x86000`, `0x87000`, `0x88000`, and `0x89000`
- Monitor end: `0x8e000`, below the guest region at `0x200000`
- EL1 vectors: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ dispatcher: `0x201000`
- First and second IRQ handlers: `0x201018` and `0x2010b0`
- Shared IRQ return: `0x201144`
- Delivery counter: `0x20117c`
- Guest end: `0x201180`, below its stack boundary at `0x3ff000`

The linked guest has one pre-unmask HPPIR read and exactly two IAR reads at
GICV offset `0x00c`. Each handler reads RPR and the full 13-bit HPPIR result
after acknowledgement, EOIR, and DIR; writes its unmodified raw IAR value to
EOIR and DIR; and restores `ELR_EL1` and `SPSR_EL1` before ERET. Every other
EL1 vector is fatal. The linker assertions keep the monitor and guest within
their fixed regions.

The only guest MMIO mappings remain the adjacent Device-nGnRE, RW, XN GICV
pages at `0xff846000` and `0xff847000`. GICD, GICC, GICH, PL011, and the
surrounding MMIO pages remain unavailable to EL1.

## Serial evidence

`baseline-serial.log.b64` losslessly preserves the raw 2679-byte pre-fix
trace. Its decoded SHA-256 is:

    0ec563066aaca6a711d6883502b1aa125c6a50723095940976933d3df348125b

`baseline-qemu.stderr.b64` preserves the 81-byte harness termination
diagnostic with decoded SHA-256:

    aee2cdab097ef2ae0fd4de0cf5c239d39f75c8af9090db98f0ea7f14cd609ce8

`serial.log.b64` losslessly preserves the final passing process's raw
7062-byte trace, including CRLF line endings. Its decoded SHA-256 is:

    5da45296fbd79ea3846738d6206767a13c87babe9db335d3d01601d24f35b988

`qemu.stderr.b64` preserves the final 81-byte harness termination diagnostic
with decoded SHA-256:

    f8a041b03b261c0a7cd7123e4f2ab3d868a45a5c97f78f19f1dc6f7dcc9039a3

The repetition loop replaces the two live logs on every iteration. The
strict oracle independently evaluated every required value on all 100
processes; the preserved passing trace is the final iteration. The SIGTERM
diagnostic results from harness cleanup after PASS, not a guest reset.
