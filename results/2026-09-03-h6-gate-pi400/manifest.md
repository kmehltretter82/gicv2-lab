# H6 hardware repetition — 36 consecutive Pi 400 boots — 2026-09-03

## Outcome

Thirty-six consecutive unattended boots of the lab image on a physical Pi 400,
each captured over serial and each compared against a frozen qemu-pi4 baseline
of the identical ELF. All thirty-six passed.

The strongest evidence here is not the pass count. Every one of the thirty-six
serial traces is **bit-identical**: hashing each trace from the
`[gicv2-lab] H4i EL2 monitor` banner to the end yields a single SHA-256,
`e12fa01e52e9f495b205c2251ee5c527`, across all thirty-six runs, at a constant
11481 bytes. Real silicon, thirty-six cold boots, no variation at all.

**This does not meet the H6 exit gate**, which is 100 consecutive boots. The
run was stopped early by request, and the attempt to extend it past 36 was
invalidated by operator error (below). The gate remains open.

## What was run

- Lab revision: working tree at `0cd1cce` plus the watchdog addition
- ELF SHA-256:
  `b29857abd3a99a1a96026263ae94bf88284ec61e123fc2399bb03e0c827d3917`
- Flat image SHA-256:
  `bf78cbdcd4af2008d342a7c85da5040c6c36b4fe6bccf83b54cff4a7216a4950`
- Scenario: `scenarios/h4i-context-save-restore-v1.json`

> **Provenance caveat.** The recorded ELF hash is *not* reproducible from the
> current tree. After this run, `src/main.c` gained an
> `#ifndef GICV2_LAB_OMIT_WATCHDOG` guard so a no-watchdog variant could be
> built for upstream QEMU. Those two preprocessor lines shift every subsequent
> line number, and the build carries `-g`, so the DWARF line table and
> therefore the ELF hash change. Rebuilding the current tree yields
> `009a6c7f11bd6aeba84128cd0c9d700ad04cb309ced5ef8b083a5dd1de662f03`.
> The **executed code is unchanged**: `llvm-objcopy -O binary` of the current
> tree reproduces the flat image hash below exactly. H5 gates on the ELF hash,
> not the flat image, so a continuation of this gate must either recapture its
> baseline from the current tree or rebuild at the pre-guard revision.
- Baseline: a qemu-pi4 capture of that same ELF, preserved here as
  `baseline-qemu-run.json` and `baseline-qemu-normalized.json` (246 records)
- Driver: `scripts/h6-repeat.sh`
- Board: Pi 400 Rev 1.0, `pi400-64`, firmware `224877da90f82a72dbcc9db10bcf059259f54680`
- Cycle time: about 65 seconds, almost all of it the vendor kernel booting

Each iteration waited for the vendor kernel over SSH, started a single-reader
serial capture, triggered `reboot '0 tryboot'`, waited for the PASS marker,
then imported the trace and compared it with the baseline. The watchdog armed
by the image performed the warm reset that returned the board to its vendor
kernel, so no physical intervention was needed for any of the thirty-six runs.

## Result

    runs 001-036 recorded: 36
    PASS: 36, FAIL: 0, TIMEOUT: 0, MISMATCH: 0
    distinct trace-body SHA-256: 1  (e12fa01e52e9f495b205c2251ee5c527, x36)
    distinct trace-body lengths:  1  (11481 bytes)

Total captured file sizes vary slightly (11935 bytes typical, one run 12066)
purely because the capture begins before the reboot takes effect and therefore
catches a variable amount of vendor-Linux console preamble on `ttyAMA0`. The
bare-metal trace itself does not vary.

`summary.tsv` holds the per-run result and `compare-NNN.json` the
`gicv2-lab.comparison.v1` record for each. Runs 001 and 036 are preserved as
raw base64 serial captures; the intermediate raw traces were not retained
because their bodies are byte-identical to those two.

## Why the run stopped at 36, and why runs 037+ were discarded

The gate was to be stopped at a round number after a request to halt it early.
While it was still running, an incorrect process check (`pgrep -fc
'h6-repeat.sh'` returned 0 for a process that `ps` later showed alive as PID
40146) led to a **second** gate being started with `START_RUN=37` into the same
output directory.

Two concurrent gates share one serial port, and `stop_capture()` kills whatever
holds that port, so each instance destroyed the other's capture mid-boot. The
symptoms were unambiguous:

| Run | Bytes | Symptom |
| --- | --- | --- |
| 037 | 6601 | contains PASS but roughly half the bytes missing — the two-reader packet split |
| 038 | 6119 | capture ends mid-line at `GICH_LR3_` |
| 039 | 5816 / 99 | reader killed almost immediately |

Duplicate run numbers 037 and 038 were appended to the summary by the two
instances. `import-trace` correctly rejected the truncated captures, so the
provenance gate did its job and no corrupt trace was ever compared as valid.

These failures are capture-side artifacts of operator error. They are **not**
hardware faults and carry no information about the board, the image, or the
GIC. Runs 037 and beyond are excluded from this result entirely; only the
uncontaminated 001-036 prefix from the single original gate is recorded here.

This is the same two-reader hazard already documented in
`docs/H6_PI400_BOOT.md` and `pi4/AGENTS.md`. `scripts/h6-repeat.sh` now takes
an atomic `mkdir` lock on `<outdir>/.gate.lock` and refuses to start when
another gate is running, which is the fix that should have existed before this
run.

## Statistical reading

By the rule of three, 36 clean runs bound the per-boot failure rate at roughly
8 % with 95 % confidence. The full 100-boot gate would tighten that to about
3 %. The bit-identical traces are the stronger argument, but they cannot
substitute for the count: a fault that appears once in fifty boots would very
likely have produced 36 identical traces too.

## Resuming

**2026-09-05 audit:** the commands below describe the historical driver. The
updated driver requires a newly prepared bundle and does not resume this
legacy directory. All 36 comparison files report a match, but only runs 001
and 036 retain raw captures here, so the claimed identity of every raw trace
cannot be independently rechecked from this archive. Both retained trace
bodies are 11766 raw bytes; converting CRLF to LF gives the documented 11481
bytes and the full SHA-256
`e12fa01e52e9f495b205c2251ee5c527d1555124458cd8b293899a232d9d84f3`.
The earlier displayed digest was abbreviated. The current ELF at the
manifest's image path also differs from the recorded hash, as noted above.
Preserve this result as historical evidence and start a fresh 100-boot gate
with `tools/h6_gate.py prepare`; keep every raw capture.

`scripts/h6-repeat.sh` accepts `START_RUN` so the remaining boots can be added
later without repeating completed ones. Because runs 037+ here are invalid, a
continuation should start from a clean output directory rather than appending
to this one:

```sh
make LLVM_BIN=/opt/homebrew/opt/llvm/bin h5-qemu H5_OUT=build/h5/gate-qemu
./scripts/h6-repeat.sh 100 build/h6-gate-2
```

The baseline must be recaptured with the fork; upstream QEMU cannot run the
watchdog-armed image at all. See
`results/2026-09-03-h4f-upstream-qualification/manifest.md`.

## Board state

The Pi 400 was returned to its original configuration after this run:
`config.txt` and `tryboot.txt` restored from backup, the lab image removed from
the boot partition, and a validation reboot confirmed the board comes up on the
vendor kernel with its original cmdline. Note that this reverts the serial
console settings, so the UART is inactive again until `enable_uart=1`,
`dtoverlay=disable-bt`, and `init_uart_clock=3000000` are re-added.
