# H6 physical Raspberry Pi 400 boot

## Scope

H6 runs the lab image on a real Pi 400 and imports the resulting serial trace
into the H5 differential runner. It is the first step in this project that
executes on hardware, and it stays opt-in: nothing here happens without the
user's explicit request.

The board is a Pi 400 Rev 1.0 (BCM2711, Cortex-A72, GIC-400). The lab already
targeted its physical addresses before any hardware existed — PL011 at
`0xfe201000` and GICD/GICC/GICH/GICV at `0xff841000`, `0xff842000`,
`0xff844000`, `0xff846000` — so no address porting was needed.

## Recoverable boot medium

The image is booted through the Raspberry Pi firmware's one-shot `tryboot`
mechanism, which is what makes a bare-metal boot safe to attempt:

1. copy the flat image to `/boot/firmware/` beside the vendor kernel;
2. write `/boot/firmware/tryboot.txt` naming it as `kernel=`;
3. `sudo reboot '0 tryboot'` from the **vendor** kernel.

`tryboot.txt` replaces `config.txt` for exactly one boot. The lab image prints
its trace and halts; the watchdog it armed then warm-resets the board, and
because the one-shot selection has already been consumed the board returns to
the vendor kernel on its own. Before the watchdog existed this required a
manual power cycle. Nothing on the card is modified except the added image
file and `tryboot.txt`; `config.txt` is backed up before editing.

`reboot '0 tryboot'` only works from the vendor kernel — this is a property of
the Raspberry Pi firmware, not of the lab.

## Serial

The trace leaves the board on the PL011, which requires three `config.txt`
settings that are easy to get wrong:

| Setting | Why |
| --- | --- |
| `enable_uart=1` | Without it the firmware never muxes GPIO14/15 to a UART at all and the header pins stay electrically idle, no matter what is attached. |
| `dtoverlay=disable-bt` | On a Pi 4/400 the PL011 is wired to the Bluetooth modem by default and the *mini*-UART is what reaches the header. This overlay swaps them, so `serial0` becomes `ttyAMA0`. |
| `init_uart_clock=3000000` | The image hardcodes `IBRD=1`/`FBRD=40`, which is 115200 baud only against a 3 MHz UART clock. The firmware default is 48 MHz. |

Setting the clock in firmware rather than changing divisors in the image is
deliberate: H5 refuses to compare traces from different ELF hashes, so the
identical binary has to run on both backends.

Wiring is GND to header pin 6 and the Pi's TXD (GPIO14, pin 8) to the
adapter's **RX**. The Pi's RXD (pin 10) can be left unconnected for capture,
which removes any possibility of driving a voltage into the board. The adapter
must be at 3V3 logic and must not supply power to the header.

Verify the mux with `pinctrl get 14,15`, which reads the hardware registers
directly and should report `a0` (`TXD0`/`RXD0`) with GPIO14 idling high:

    14: a0    pn | hi // GPIO14 = TXD0
    15: a0    pu | hi // GPIO15 = RXD0

Note that the kernel's own `/sys/kernel/debug/pinctrl/*/pinmux-pins` reports
these pins as `(MUX UNCLAIMED)` even when they are correctly muxed, because
the firmware programs the mux and no Linux driver claims it. Do not read that
as a fault.

### One reader, always

Capture with exactly one process on the tty. Two readers do not produce an
error; they silently split the stream, each `read()` taking alternate USB
packets, which yields a clean-looking trace that is missing half its bytes in
roughly 62-byte runs. Because the port name lives in the shell redirection
rather than in `cat`'s command line, `pkill -f <port>` matches the subshell
and can leave an orphaned reader behind. Check with `lsof` on the port before
starting a capture and confirm the byte count afterwards.

## Boot sequence on hardware

The firmware's armstub drops to EL2 (non-secure) and enters the image at
`0x00080000` with the other three cores held in its spin table. The trace's
first field, `CurrentEL=2`, is what confirms this; the monitor fails loudly
and halts if it is anything else.

Everything from H1 through H4i then runs unchanged: the deliberate `BRK`,
stage-2 enable, GIC discovery from `GICH_VTR`, the EL1 guest, and the paused
virtual-interface context save and restore.

## A hardware hazard that did not fire

`stage2_enable()` programs `VTCR_EL2` with `IRGN0`/`ORGN0` set to write-back
write-allocate, so the stage-2 table walker performs **cacheable** reads. The
EL2 monitor runs with `SCTLR_EL2.M` clear, so its own descriptor writes are
Device-nGnRnE and bypass the caches. On real silicon a stale cache line for
those physical addresses could therefore be visible to the walker but not to
the writer, which would corrupt translation.

This did not occur on the 2026-09-03 run. It is recorded because TCG models no
caches and can never exercise it, so the QEMU gates provide no evidence either
way. If a future hardware boot faults or hangs at or shortly after
`stage2_root`, clean the table region to the point of coherency before writing
`VTTBR_EL2` rather than changing `VTCR_EL2`; the cache maintenance is
invisible to the trace, whereas changing the walker's cacheability would alter
a compared field and break the differential.

## Importing the trace

The captured serial file is imported without performing any further board
action:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py import-trace \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --backend pi400 --backend-id pi400-64-gic400-tryboot-20260903 \
  --image build/gicv2-lab.elf \
  --serial /path/to/captured-serial.log \
  --out results/YYYY-MM-DD-h6-pi400
```

`--image` must be the ELF the flat image was generated from; the runner hashes
it and refuses to compare against a QEMU capture of different bytes. Verify
that the flat image on the boot partition still hashes to the `objcopy` output
of that ELF before trusting the import.

Then compare against a QEMU capture of the same ELF:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py compare \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --left build/h5/qemu/normalized.json \
  --right results/YYYY-MM-DD-h6-pi400/normalized.json \
  --out results/YYYY-MM-DD-h6-pi400/comparison-qemu-vs-pi400.json
```

A clean comparison should always be accompanied by a check that it was not
vacuous: confirm the normalized record count, and mutate one imported field to
confirm the comparison then fails.

## Recorded result

The first physical boot ran on 2026-09-03 from lab revision
`0cd1cce793e867c75a6fdd058258432342a0603f`. The unmodified H4i image reached
`[gicv2-lab] H4i PASS` on hardware, and its trace compared equal to a qemu-pi4
capture of the identical ELF across 25 ordered markers and 221 architectural
field values.

Six raw lines differ, all identity or capability registers the scenario
excludes by design. Two are fork-fidelity observations worth following up —
the physical `GICC_PMR` priority width and `VTCR_EL2` bit 31 at reset — and
neither has been qualified for a report. The full evidence, hashes, boot
configuration, and both raw traces are in
`results/2026-09-03-h6-pi400/manifest.md`.

## qemu-pi4 fidelity observations from the first differential

The H4i comparison is clean, but six raw trace lines differ, all of them
identity or capability registers the scenario excludes as incidental. Three of
those six trace back to specific properties the fork's GIC instantiation in
`hw/arm/bcm2838.c` does not set, and the root cause is worth recording even
though none of it is qualified for an upstream report.

`bcm2838.c` sets `revision`, `num-cpu`, `num-irq`, and
`has-virtualization-extensions` on its `arm_gic`, and nothing else. The two
unset properties fall back to QEMU defaults from `arm_gic_common.c`:

| Property | fork value | Pi 400 GIC-400 | Observable in the trace |
| --- | --- | --- | --- |
| `num-irq` | `192 + 32 = 224` | 256 | `GICD_TYPER` ITLinesNumber 6 against 7 |
| `num-priority-bits` | 8 (default, unset) | 5 | `GICC_PMR` reads `0xff` against `0xf0` |
| `has-security-extensions` | false (default, unset) | true | `GICD_TYPER.SecurityExtn` 0 against 1 |

The priority-bit difference is fully explained by those two defaults acting
together. On hardware the GIC-400 implements 5 priority bits, so a write of
`0xff` is stored as `0xf8`; because the Security Extensions are implemented and
the lab runs non-secure, the read back returns the Non-secure view
`(0xf8 << 1) & 0xff`, which is exactly the `0xf0` observed. Under the fork
neither mechanism is active — 8 implemented bits and no Non-secure shift — so
the same write reads back unchanged as `0xff`.

Note the asymmetry this creates. `gic_fullprio_mask()` in `hw/intc/arm_gic.c`
hardcodes `GIC_VIRT_MAX_GROUP_PRIO_BITS` for a vCPU interface and only consults
`s->n_prio_bits` for a physical one:

```c
if (gic_is_vcpu(cpu)) {
    priBits = GIC_VIRT_MAX_GROUP_PRIO_BITS;
} else {
    priBits = s->n_prio_bits;
}
```

So the fork models the *virtual* interface's priority width correctly —
`GICH_VTR` is `0x90000003` on both backends, decoding to 4 List Registers and
5 preemption and priority bits — while the *physical* CPU interface is 8 bits
wide. Every H4 scenario to date exercises virtual priorities, which is why
this never reached a compared field. A scenario that sets physical priorities,
or one that depends on Group 0/1 separation, would diverge.

None of this is a defect claim. GICv2 leaves the number of implemented
priority bits and the presence of the Security Extensions implementation
defined, so the fork is not violating the architecture; it is modelling a
different GIC than the one the Pi 400 has. Per AGENTS.md a QEMU/Pi/KVM
difference is evidence to investigate rather than proof of a defect, and none
of this has been reproduced against unmodified upstream QEMU or checked for
prior reports.

## Watchdog and unattended repetition

The image arms the BCM2711 watchdog (`src/watchdog.c`) once, early in
`lab_main`, for its maximum interval of `0xfffff` ticks at 65536 Hz — just
under 16 seconds. This is the lab's only MMIO outside the UART, timer, and GIC
blocks, and it is what makes a hardware repetition gate possible at all:

- the monitor prints its trace and halts, as H6 requires;
- the watchdog then performs a warm reset;
- the firmware's one-shot `tryboot` selection has already been consumed, so
  the board comes back on the normal `config.txt` and its vendor kernel;
- the next iteration can start over SSH with no physical intervention.

It also bounds a hang. A halted board cannot be recovered over a serial line,
and a power cycle is the only alternative.

The same binary arms the watchdog under QEMU, which models the BCM2711 power
management block faithfully (`hw/misc/bcm2835_powermgt.c`: same password byte,
same `RSTC`/`WDOG` offsets, same 65536 Hz tick, and a real reset request). The
timeout is never reached there because `capture_qemu` stops at the success
marker within a few seconds and its default timeout is 10 seconds, below the
watchdog's 16. `scripts/smoke-qemu.sh` additionally passes `-no-reboot` and
asserts that the PASS line appears exactly once, so a watchdog-induced re-run
would fail loudly rather than silently doubling a trace.

The two lines the image prints about arming — `[gicv2-lab] watchdog armed for
full reset` and `watchdog_ticks_armed=1048575` — are deliberately neither a
scenario marker nor a compared field. `normalize_trace` skips any line that is
not the next expected marker or a key in `expected_keys`, so they appear
identically in both backends' raw traces and affect no comparison.

## Repetition gate

Prepare a new gate **locally**, before any board action. This snapshots the
working source, records the base commit and toolchain versions, builds a new
image inside the snapshot, and captures its QEMU baseline. Existing output
directories are rejected. Use the fork because this image arms the watchdog.

```sh
python3 tools/h6_gate.py prepare --out build/h6-frozen \
  --llvm-bin /opt/homebrew/opt/llvm/bin \
  --qemu ../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64
python3 tools/h6_gate.py verify --gate build/h6-frozen
```

`gate.json` pins every source, build, and QEMU artifact. The default target is
100 boots; `prepare --runs N` selects a different experimental count. A ready
bundle has zero hardware runs and provides no hardware evidence.

After an explicit hardware request, deploy exactly the bundle's
`image/gicv2-lab.bin` using the boot configuration above. The driver checks the
deployed image's SHA-256 and requires the five-setting `tryboot.txt` above
to select that same file before each reboot. Run the frozen driver:

```sh
PI_HOST=user@board PORT=/dev/cu.serial \
  bash build/h6-frozen/source/scripts/h6-repeat.sh 100 build/h6-frozen
```

Host and port must be supplied explicitly. `REMOTE_IMAGE` defaults to
`/boot/firmware/gicv2-lab-h6.bin` and must stay in `/boot/firmware`;
`TRACE_TIMEOUT` and `LINUX_TIMEOUT` default
to 120 and 180 seconds. The image, scenario, and baseline come exclusively
from the verified bundle. A cycle previously took about 65 seconds.

The driver takes separate board, serial-device, and output-directory locks.
The resource locks share `/tmp/gicv2-lab-h6-<uid>` across result directories;
macOS `tty.*` and `cu.*` aliases share a serial lock. Use a consistent board
hostname and lock root across invocations. An existing serial reader is
reported and left alone. Only the capture process owned by this invocation
is terminated, including on SIGINT/SIGTERM. A SIGKILL may leave a lock that
must be inspected and removed manually after its owner has exited.

Each attempt has an exclusive `runs/NNN` directory containing the raw serial
capture, board environment and boot configuration, deployed-image hash,
commands, stderr, H5 import, comparison, and result. All raw traces are kept,
including identical traces and failures. The driver stops at the first failed
attempt. It never truncates or replaces an existing run.

`START_RUN=N` resumes only after `N-1` consecutive complete passing attempts
whose saved evidence still verifies. A failed or interrupted attempt requires
a new campaign. The old 2026-09-03 result directory predates this format and
cannot be resumed by this driver; begin a fresh 100-boot gate.

Archive the complete evidence locally, including a partially completed gate:

```sh
python3 tools/h6_gate.py archive --gate build/h6-frozen \
  --out results/h6-frozen.tar.gz
```

The archive is relocatable and retains all image and source bytes even though
the repository ignores loose build files. Extract it into a new directory
and run `verify --gate /path/to/extracted/gate` to audit it. Never remove raw
captures on the grounds that their normalized traces match.

## Exit gate

The H6 exit gate is 100 consecutive fresh hardware boots whose traces each
compare equal to a frozen qemu-pi4 baseline of the identical ELF, with no
timeout, no FAIL marker, and no provenance rejection. See the recorded result
directory for whether it has been met and with which revision.
