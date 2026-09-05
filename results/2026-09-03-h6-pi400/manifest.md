# H6 first physical Pi 400 boot and first QEMU/hardware differential — 2026-09-03

## Outcome

The unmodified H4i lab image booted bare metal on a physical Raspberry Pi 400
and ran the complete H1/H2/H3/H4i sequence to `[gicv2-lab] H4i PASS` on a real
GIC-400 virtualization interface.

The same ELF bytes were then captured under qemu-pi4 TCG and compared with the
hardware trace through `tools/h5_runner.py`. The comparison reported
`matches: true` with zero differences over 246 normalized records — 25 ordered
markers and 221 architectural field values.

This is the first result in this lab that compares QEMU with physical
hardware. It produces no QEMU bug candidate for the compared field set. It
does record two fork-fidelity observations outside that set, listed below,
which are evidence to investigate rather than defects.

## Safety boundary

The run satisfied the ROADMAP H6 constraints:

- one-shot `tryboot` boot medium, fully recoverable: `tryboot.txt` replaces
  `config.txt` for exactly one boot, and the next reset returned the board to
  the vendor kernel `6.18.39+rpt-rpi-v8` unaided;
- no runtime storage, network, USB, PCIe, OTP, or firmware writes by the lab
  image — its only MMIO is the PL011 UART, the GIC-400 GICD/GICC/GICH/GICV
  blocks, and the EL2 physical timer;
- one core: `arch/arm64/start.S` parks every CPU whose `MPIDR_EL1[7:0]` is
  non-zero in `wfe`, and the firmware's armstub holds the other three in its
  spin table;
- the guest completes HVC calls and virtual interrupts, then the monitor
  prints the trace and halts in `wfe`;
- SMP, randomized sequences, repeated resets, guest-controlled MMIO, and
  stage-2 fuzzing all remained disabled.

The board was returned to the vendor kernel by a power cycle after each boot,
because the image halts as specified.

### Deviation from the ROADMAP H6 sketch

The ROADMAP describes the first hardware image as completing "one HVC and one
virtual interrupt". The image booted here is the full, unmodified H4i payload,
which does that and considerably more. This was deliberate:

- it required no source change, so no new code was introduced on the first
  physical boot;
- its ELF hash is identical to the QEMU baseline, which is what
  `h5_runner.py`'s provenance gate requires for a differential comparison;
- it prints incrementally, so a failure would have localized itself on the
  serial trace.

The approved watchdog (`0xfe100000`) was **not** armed and remains unused. Once
the UART was working, a hang would have been directly observable on the wire,
so the watchdog added MMIO outside the ROADMAP restriction and changed the ELF
hash for no evidence gain. It stays available for a future unattended
repetition gate, where automatic reset back to the vendor kernel is what makes
repetition possible.

## Lab image

- Lab source revision: `0cd1cce793e867c75a6fdd058258432342a0603f`
- Source worktree: clean at build time
- ELF SHA-256:
  `0bef6082deb828d5b423bc5e081965f05bfa601eaced8dc853bee554541ddec2`
- Flat image SHA-256 (what the firmware loaded):
  `9241a235f93402cebddbe6bced4f4fdf121b0f05bb9ba6913db66008a63e73cb`
- The flat image is `llvm-objcopy -O binary` output of that exact ELF; the
  hash on the boot partition was verified equal to the locally generated one
  before booting.
- Toolchain: Homebrew clang version 22.1.8, `--target=aarch64-none-elf`,
  `-mcpu=cortex-a72`
- Build host: macOS 14.8.7 arm64

## Hardware environment

- Raspberry Pi 400 Rev 1.0, BCM2711, Cortex-A72, GIC-400
- Hostname `pi400-64`, Raspberry Pi OS trixie, arm64 userland
- Vendor kernel `6.18.39+rpt-rpi-v8` (the tryboot host, not the tested code)
- Bootloader/firmware: `224877da90f82a72dbcc9db10bcf059259f54680` (release),
  dated 2026/05/17
- Reached over `wlan0` at `192.168.1.15`; `eth0` was disconnected
- Serial: PL011 `0xfe201000` on GPIO14/15 (header pins 8 and 6), FT232 to the
  build host at 115200 8N1, no flow control
- Backend ID recorded in `run-pi400.json`: `pi400-64-gic400-tryboot-20260903`

### Boot configuration

`tryboot.txt` in this directory is the exact one-shot configuration used:

    arm_64bit=1
    enable_uart=1
    dtoverlay=disable-bt
    init_uart_clock=3000000
    kernel=gicv2-lab-h6.bin

`enable_uart=1` is what muxes GPIO14/15 to a UART at all; `disable-bt` moves
the PL011 off the Bluetooth modem onto the header; and `init_uart_clock`
of 3 MHz matches the image's hardcoded `IBRD=1`/`FBRD=40`, which yields
115384 baud against a 115200 receiver — a 0.16 % error, well inside tolerance.
Setting the clock in firmware rather than changing the divisors in the image
is what allows one ELF to run on both backends.

The firmware handed off at EL2: the trace records `CurrentEL=2`.

## Comparison

- Runner: `tools/h5_runner.py`, `RUNNER_VERSION=1`
- Scenario: `scenarios/h4i-context-save-restore-v1.json`
- Scenario SHA-256:
  `54e8d2ad32f29350a15d6159c2e026e854cefc3222d32b5732efb8284ce3e2e9`
- QEMU: `qemu-system-aarch64` 11.1.0 (`v0.1.0-bootstrap-113-ge13553c232-dirty`)
- qemu-pi4 revision: `7dc888115acb285305bfd137921f7a1d81724863`
- QEMU binary SHA-256:
  `371c90f9fc3722fc04936e1c36f9516268a200e12fec1f7091b0fb84bba57a60`

Both `run-qemu.json` and `run-pi400.json` record the same image SHA-256, so the
runner's same-payload provenance gate genuinely applied rather than being
bypassed.

`comparison-qemu-vs-pi400.json` reports `matches: true` and an empty
`differences` array, exit status 0.

### The comparison is not vacuous

Two checks were performed so that "no differences" cannot be confused with
"nothing was compared":

1. The normalized traces contain 246 records each — 25 `marker` records and
   221 `field` records.
2. A negative control mutated exactly one imported hardware field,
   `GICH_LR0_low_active`, from `0x28000032` to `0xdeadbeef`. The comparison
   then exited 1 and named that single field as a `field-value-mismatch`.

## Differences outside the compared field set

Six raw trace lines differ between the two backends. All six are identity or
capability registers that the scenario deliberately excludes as incidental
backend fingerprints, which is why the semantic comparison is clean. They are
recorded here as evidence.

| Field | qemu-pi4 | Pi 400 | Assessment |
| --- | --- | --- | --- |
| `ID_AA64PFR0_EL1` | `0x0222` | `0x2222` | Expected. The `EL3` field is 2 on hardware and 0 under `-cpu cortex-a72,has_el3=off`. |
| `VTCR_EL2` (reset) | `0x0` | `0x80000000` | Bit 31 is RES1 in `VTCR_EL2`. Hardware reads it set at reset; QEMU reads 0. Fidelity observation. |
| `GICD_TYPER` | `0x0066` | `0xfc67` | Hardware: 256 INTIDs, `SecurityExtn=1`, `LSPI=31`. QEMU: 224 INTIDs, `SecurityExtn=0`, `LSPI=0`. Both report 4 CPUs. |
| `GICD_IIDR` | `0x043b` | `0x0200143b` | Implementer/revision. Excluded by design. |
| `GICC_IIDR` | `0x0002043b` | `0x0202143b` | Implementer/revision. Excluded by design. |
| `GICC_PMR_enabled` | `0xff` | `0xf0` | After writing `0xff`, hardware exposes 4 writable physical priority bits, QEMU 8. Fidelity observation. |

Two of these are worth following up as qemu-pi4 fidelity observations rather
than defects, because GICv2 leaves the affected quantities implementation
defined and neither affected the compared state:

- **Physical priority width.** `GICH_VTR` is `0x90000003` on *both* backends,
  which decodes to 4 List Registers, 5 preemption bits, and 5 priority bits,
  so the fork models the *virtual* interface's priority width exactly. The
  *physical* `GICC_PMR` readback nonetheless differs, 8 writable bits against
  the hardware's 4. The scenario exercises virtual priorities, so this did not
  reach the compared trace; a scenario that sets physical priorities would
  diverge. The exact mechanism on hardware — 5 implemented bits with the
  non-secure view losing one, against a genuinely narrower field — has not
  been established here and should be before anything is reported.
- **`VTCR_EL2` bit 31.** The architecture defines it RES1. Whether a reset read
  must return 1 needs to be checked against the ARM ARM text before this is
  called a defect.

Neither has been qualified for an upstream report. Per AGENTS.md, a
QEMU/Pi/KVM difference is evidence to investigate, not proof of a defect, and
nothing here was reproduced on unmodified upstream QEMU or checked for prior
reports.

## Capture integrity

The first boot (`pi400-serial-boot1-partial.log.b64`) is preserved but is
**incomplete evidence**: it lost 50.4 % of its bytes. The loss was a host-side
capture fault, not a hardware or image fault. Kept and dropped runs both
averaged about 62 bytes and alternated, matching the FT232's 64-byte USB bulk
packet payload; two `cat` processes were reading the same tty and each `read()`
took alternate packets. The stale reader survived an earlier `pkill` whose
pattern matched only the subshell, because the port name appears in the shell
redirection rather than in `cat`'s own command line.

After removing the stale reader, a sustained control transfer of 250 numbered
48-byte lines from vendor Linux was captured with zero loss, and the second
boot produced a trace byte-identical in length to the QEMU reference (11410
bytes each, 99.91 % character similarity before normalization).
`pi400-serial.log.b64` is that second, complete boot and is the trace imported
into H5.

## Files

| File | Contents |
| --- | --- |
| `pi400-serial.log.b64` | Lossless raw serial capture of the compared boot. |
| `pi400-serial-boot1-partial.log.b64` | First boot, 50.4 % byte loss, retained for provenance only. |
| `qemu-serial.log.b64` | Raw serial capture of the qemu-pi4 run. |
| `run-pi400.json` | `gicv2-lab.run.v1` provenance for the hardware trace. |
| `run-qemu.json` | `gicv2-lab.run.v1` provenance for the QEMU trace. |
| `normalized-pi400.json` | `gicv2-lab.trace.v1` projection of the hardware trace. |
| `normalized-qemu.json` | `gicv2-lab.trace.v1` projection of the QEMU trace. |
| `comparison-qemu-vs-pi400.json` | `gicv2-lab.comparison.v1`, `matches: true`. |
| `tryboot.txt` | The exact one-shot boot configuration used. |

## What this does not establish

A single boot on one board is not a repetition gate. The QEMU-side milestones
each required 100 consecutive fresh processes; this result is one hardware
boot compared against one QEMU capture. It also covers only the H4i scenario,
and only this GIC-400 revision on this firmware. It says nothing about KVM.
