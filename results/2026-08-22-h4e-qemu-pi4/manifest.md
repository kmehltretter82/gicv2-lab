# H4e qemu-pi4 result — 2026-08-22

## Outcome

The strict H4e smoke oracle passed 100 consecutive fresh QEMU processes.
Every process reached H1 PASS, H2 PASS, and H4e PASS and satisfied every
exact-value and single-occurrence check in `scripts/smoke-qemu.sh`.

EL1 acknowledged software virtual INTID 42 twice. During the first active
report, EL2 performed the scenario's sole re-pend and changed LR0 from Active
`0x2800002a` to Pending+Active `0x3800002a`. The first EOIR dropped priority
without changing that state, and HPPIR remained the spurious INTID 1023. The
first DIR then exposed Pending `0x1800002a`, at which point HPPIR returned 42.
The second acknowledgement changed LR0 back to Active; the second EOIR and
DIR completed it to Invalid `0x0800002a`, after which EL2 cleared LR0.

Across all checkpoints, HCR stayed `0x1`, VMCR stayed `0xf85c0201`, LR1
stayed zero, MISR and both EISR banks stayed zero, and EOICount stayed zero.
No forbidden outcome, maintenance interrupt, or physical IRQ was observed.
This is a result for the exact system below, not a general claim that all
GICv2 virtualization behavior is correct.

The observed result matches the frozen architectural contract. H4e therefore
produced no divergent behavior and no QEMU upstream bug candidate.

## Lab image

- Source revision: `78f080a7db62784521ac7fd0c2ef331cda638e71`
- Source worktree: clean during the recorded run
- ELF: `build/gicv2-lab.elf`, 190376 bytes
- ELF SHA-256: `916e79af96c6cbc7db6bdeded2358307a338dfb8cb84171c2f112007b34fd4ab`
- Raw image: `build/gicv2-lab.bin`, 1577352 bytes
- Raw-image SHA-256: `1b321dde101f788fd794755e263924f2ba3b5e05c51056891c571cf69cce47e2`

The image was clean-built from the recorded revision immediately before the
gate.

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source checkout: `fe4019fdebf5452763ee8254c1ae8c8c46b9adbb`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-17-gfe4019fdeb-dirty)`
- QEMU binary SHA-256: `b371a29a92d18c494a089f558e33b19c14d8f8c08b0047cfac9899b48aa4603a`

The QEMU executable is byte-identical to the binary recorded for the H3
through H4d gates. The QEMU worktree's tracked modifications were confined to
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
- Stage-2 tables: `0x86000`, `0x87000`, `0x88000`, and `0x89000`
- Monitor end: `0x8e000`, below the guest region at `0x200000`
- EL1 vector table: `[0x200800, 0x201000)`, 2 KiB and 2 KiB-aligned
- Guest IRQ dispatcher: `0x201000`
- First and second IRQ handlers: `0x201018` and `0x2010b4`
- Shared IRQ return restoring `ELR_EL1` and `SPSR_EL1`: `0x20114c`
- Guest delivery counter: `0x201184`
- Guest end: `0x201188`, below its stack boundary at `0x3ff000`

The disassembly audit confirmed that every EL1 vector slot allocates a
64-byte frame and saves `x0`-`x3`, `ELR_EL1`, and `SPSR_EL1` before branching.
The linked guest contains exactly two IAR reads at GICV offset `0x00c`, one in
each distinct handler. Each handler reads RPR at `0x014` and HPPIR at `0x018`
after acknowledgement, EOIR, and DIR. The first path uses HVC operations
`0x106`, `0x105`, and `0x107`; the second uses `0x108`, `0x109`, and `0x10a`.
Both paths write EOIR at `0x010` and DIR at `0x1000`, then converge on the
return that restores both exception-return registers before ERET. The first
path stores delivery count 1 and the second stores 2; EXIT is gated on 2.
The linker enforces the monitor and guest upper bounds with assertions.

The guest-visible GICV mappings were exactly the two adjacent Device-nGnRE,
RW, XN pages required for the architectural 8 KiB CPU-interface block:

- `0xff846000`: descriptor `0x00400000ff8467c7`
- `0xff847000`: descriptor `0x00400000ff8477c7`

The stage-2 table is zero-filled before those two entries are installed, so
GICD, GICC, GICH, PL011, and the surrounding MMIO pages remain unavailable to
EL1.

## Serial evidence

`serial.log.b64` is a Base64 encoding of the final process's raw, unmodified
7090-byte serial log, including its CRLF line endings. Its decoded SHA-256 is:

    4bf0f3f595133760a8c8efa132d1d11174a6cadec31ad311107d46bd36899fbd

The repetition loop intentionally replaces the log on every iteration. The
strict oracle independently evaluated the complete required register and
checkpoint set for each of the 100 processes; the preserved raw log is the
last passing iteration.

After accepting the final PASS marker, the smoke harness terminated the
halted QEMU process with SIGTERM. `qemu.stderr.b64` losslessly preserves its
81-byte termination diagnostic; its decoded SHA-256 is
`b951452959973b38dfbbe0c0cf0b7d29a8024c642547135e4a2f4ee4238bd0a7`.
That harness cleanup diagnostic is not a guest reset or failure.
