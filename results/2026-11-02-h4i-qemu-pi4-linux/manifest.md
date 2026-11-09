# H4i qemu-pi4 Linux-host reproduction — 2026-11-02

## Outcome

The current lab image passed the H4i smoke oracle on a qemu-pi4 build made
from source on a fresh Linux x86_64 host: one `make smoke` run and 20
consecutive fresh processes under `make SMOKE_RUNS=20 smoke-repeat`.

This is a portability check of the lab and fork on a second host and
toolchain. It is not a re-run of the frozen 2026-08-22 H4i gate: the lab
revision, compiler, QEMU revision, and QEMU configuration all differ from
`results/2026-08-22-h4i-qemu-pi4/`, so the image and emulator hashes differ
too. No physical Pi boot, boot-medium change, or external report was made.

## First attempt: wrong fork revision

The fork was first built at `8460833e53a91458fd3ba63ff19fb6c4932e8bb4`, the
revision pinned by the 2026-08-22 H4i result. The current image stopped
after its banner. QEMU logged `bcm2835_powermgt_write: WDOG` as
unimplemented and reset the machine immediately on the image's `PM_RSTC`
write; `-no-reboot` turned that reset into a clean exit.

That revision predates fork commit `0784975a` ("hw/misc: model BCM2835 PM
watchdog countdown"). The image has armed the watchdog since lab commit
`06d07da`, and `results/2026-09-03-h4f-upstream-qualification/` already
records that only a fork with the countdown can run it. As a control, lab
revision `0e4374701ff73860a300e20ba76c75d45cf14d04`, which has no watchdog,
was rebuilt in a temporary worktree and passed `make smoke` on the
`8460833e` build. This was a revision-selection error, not a new QEMU or lab
defect.

## Lab image

- Source revision: `57ebb2873169498fd0ea6d66eb78add700c83228`
- Source worktree: clean during the build and all smoke runs
- ELF: `build/gicv2-lab.elf`, 201952 bytes
- ELF SHA-256: `131decaf5d26d1e74e68e4366182cff4ba4fee32618d9b51e2f8e9311252cc85`
- Raw image: `build/gicv2-lab.bin`, 1577472 bytes
- Raw-image SHA-256: `71e742dc15764499a16adade437726837aded81c5cdf215200fbc888cda28ab4`

## System under test

- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU repository: `kmehltretter82/qemu-pi4`
- QEMU source revision: `a28cb0859f29bcae333123f516e58306fc863937`, clean
- QEMU version: `11.1.0`
- QEMU binary SHA-256: `12a8c9898d60d5c3f2a77e25e2ae351176662a07fcf1966b55fd83c1c843a5f7`

The fork does not record its own configure line. This build used:

    mkdir build-pi4-native-fdt && cd build-pi4-native-fdt
    ../configure --target-list=aarch64-softmmu --enable-fdt=system \
        --disable-docs --disable-tools --disable-werror
    ninja qemu-system-aarch64

Configure reported `fdt: system` against Ubuntu libfdt 1.7.0. The binary hash
is the authoritative emulator identity.

## Toolchain and host

- Clang: Ubuntu clang 18.1.3 (1ubuntu1)
- Linker: Ubuntu LLD 18.1.3
- llvm-objcopy/llvm-objdump: Ubuntu LLVM 18.1.3
- Host: Ubuntu 24.04.5 LTS, Linux 6.18.44, x86_64, 4 CPUs, ephemeral
  container
- Date: 2026-11-02

## Commands

    make -j4 all
    make smoke
    make SMOKE_RUNS=20 smoke-repeat

The default `QEMU` path,
`../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64`, pointed at
the build above. Each process used the invocation recorded in
`scripts/smoke-qemu.sh`.

## Trace comparison with the 2026-08-22 H4i trace

Every difference from the frozen H4i trace has an identified cause:

| Field | 2026-08-22 | This run | Cause |
| --- | --- | --- | --- |
| watchdog lines | absent | two added lines | Lab `06d07da` arms the watchdog; neither line is a scenario marker |
| `ELR_EL2` (expected BRK) | `0x84048` | `0x8416c` | Image layout; `0x8416c` is the `brk #0x471` in this build's disassembly |
| `GICD_TYPER` | `0x66` | `0xfc67` | Fork `06f4fd98` adopts the Pi 400 GIC-400 value |
| `GICD_IIDR` | `0x43b` | `0x0200143b` | Fork `06f4fd98` |
| `GICC_IIDR` | `0x2043b` | `0x0202143b` | Fork `06f4fd98` |
| `GICC_PMR_enabled` | `0xff` | `0xf0` | Fork `06f4fd98`; the smoke oracle expects `0xf0` since lab `57ebb28` |

The new GIC values match the Pi 400 values in
`results/2026-09-03-h6-pi400/` and `results/2026-10-03-h8-pi400-kvm/`. All
other lines, including every LR, APR, VMCR, MISR, and EISR snapshot, are
byte-identical.

## Serial evidence

`serial.log.b64` losslessly preserves the raw 11768-byte serial trace,
including CRLF line endings. Its decoded SHA-256 is:

    63da01cf95f1563c0b32801c39f526fe1a00e7e2f102ac9affd8740c6241655a

The single `make smoke` run and the final `smoke-repeat` iteration produced
this same hash. The repetition loop replaces the live log each iteration, so
only the final trace is preserved.

`qemu.stderr.b64` preserves the 71-byte harness termination diagnostic with
decoded SHA-256:

    7f1d19f4e9457cf0f4523f6776ebb313ae7a3aefa711e76c1125a949e876056f

The SIGTERM diagnostic is expected harness cleanup after PASS, not a guest
reset.
