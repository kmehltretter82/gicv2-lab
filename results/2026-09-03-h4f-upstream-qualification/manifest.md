# H4f upstream qualification, and the first fork-versus-upstream differential — 2026-09-03

## Outcome

Two separate results, both QEMU-only. No hardware, network, storage, or
firmware action was performed, and nothing was reported anywhere.

1. **The H4f virtual-SGI source-tag defect reproduces on unmodified upstream
   QEMU.** It was previously recorded only as a qemu-pi4 fork defect candidate.
   It is now reproduced on an unmodified upstream release binary, and the
   responsible code is confirmed present on upstream master as of 2026-09-02.
2. **The H4i contract compares equal between the fork and unmodified upstream.**
   Running the identical ELF on both under `raspi4b`, the H5 comparison reports
   `matches: true` with zero differences.

## 1. H4f reproduces upstream

### Payload

- Lab revision: `720149dbd00dc4eecb390125fd54afe5ce4fcc0c`, the frozen H4f
  implementation, built in a detached worktree so the H6 gate's tree was not
  disturbed.
- ELF SHA-256:
  `ae78b410d9df46fabb567b6001794965f1b96d3f81390f4d5b5328c83e2c0391`
- The identical ELF was run on both emulators.

### Backends

| | Binary | Version |
| --- | --- | --- |
| fork | `qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64` | 11.1.0 (`v0.1.0-bootstrap-113-ge13553c232-dirty`), fork revision `7dc888115acb285305bfd137921f7a1d81724863` |
| upstream | `/opt/homebrew/bin/qemu-system-aarch64` | 11.0.2, unmodified release build |

Both were invoked as `-machine raspi4b -cpu cortex-a72,has_el3=off -no-reboot`.

### The divergence

    fork     GuestHPPIR_both_pending_observed=0x0000000000000405
    upstream GuestHPPIR_both_pending_observed=0x0000000000000005

The difference is exactly `0x400`, which is CPUID 1 in `GICV_HPPIR` bits
`[12:10]`. Upstream drops the SGI source CPUID; the fork reports it. The
matching List Register at that checkpoint is `GICH_LR0=0x22000405`, whose
VirtualID field is 5 and whose CPUID field is 1, so `0x405` is the value the
architecture requires.

Upstream then fails the scenario's own oracle at the next checkpoint:

    [gicv2-lab] H4f FAIL: invalid initial SGI HPPIR report

and the trace stops after 2679 bytes against the fork's 7062. The fork run
reaches H4f PASS.

### Root cause in upstream

`hw/intc/arm_gic.c`, `gic_cpu_read()`:

```c
case 0x18: /* Highest Pending Interrupt */
    *data = gic_get_current_pending_irq(s, cpu, attrs);
    break;
```

`gic_get_current_pending_irq()` returns the bare INTID. For a virtual CPU
interface the SGI source CPUID must come from the CPUID field of the matching
List Register. `gic_acknowledge_irq()`, which serves `GICV_IAR`, already does
this, so upstream's `GICV_IAR` and `GICV_HPPIR` disagree about the same
pending virtual SGI: IAR carries the source tag and HPPIR does not.

The controlling rules are sections 5.3.8 and 5.5.7 of the Arm Generic
Interrupt Controller Architecture Specification version 2.

### Still present on current master

`upstream/master` at `a925240509` (2026-09-02, fetched 2026-09-03) still
contains the same `case 0x18` calling `gic_get_current_pending_irq()`, and
contains no HPPIR helper handling the SGI source. Searching upstream history
finds no commit addressing this: the only commits touching
`gic_get_current_pending_irq` are `c5619bf9e8` ("Change behavior of IAR
writes") and `7c0fa108d9` ("Handle grouping for GICC_HPPIR"), and the latter
concerns the physical interface's group handling, not the virtual SGI source
field.

### Qualification status

| Requirement | Status |
| --- | --- |
| Reproduces on unmodified upstream | **Done** — release binary 11.0.2, and code confirmed on master `a925240509` |
| Prior-report research | Upstream git history checked, no fix found. qemu-devel list search outstanding. |
| User review before any report | **Not done. Nothing has been sent anywhere.** |

### Provenance constraint on the fix

The fork's fix, `8460833e53a91458fd3ba63ff19fb6c4932e8bb4`, carries an
`Assisted-by: OpenAI Codex` trailer. AGENTS.md forbids turning AI-assisted work
from this repository into a QEMU upstream patch, and QEMU's own code-provenance
policy must be satisfied independently. That constraint applies to the *patch*.
A defect report describing the observed behaviour and a reproducer is a
separate decision, and remains the user's to make and review.

## 2. H4i compares equal between fork and upstream

This is the comparison the README lists as the project's first purpose and
which had not previously been run.

- Payload: the current H4i image built with `-DGICV2_LAB_OMIT_WATCHDOG`,
  ELF SHA-256
  `e9ad4ca1e831e659ff8e06cf8035d69c3d5fdc6b21db7577ad7f33989756595a`
- Scenario: `scenarios/h4i-context-save-restore-raspi4b-v1.json`, added for
  this comparison. It is the H4i semantic contract with `machine` set to
  `raspi4b` instead of `raspi400`, because unmodified upstream has no
  `raspi400` machine and both sides must use the same invocation.
- Result: `matches: true`, empty `differences`, and both `run.json` files
  record the same image hash, so the provenance gate applied.

### Why the watchdog had to be omitted

The current H6 image arms the BCM2711 watchdog. On upstream QEMU that write
resets the machine **immediately**: upstream's `bcm2835_powermgt.c` calls
`qemu_system_reset_request()` on any RSTC write with the reset bit and logs
`WDOG` as unimplemented, so no countdown is modelled. The image therefore
never gets past its banner on upstream, and produced only 31 bytes of trace.

The fork does model the countdown, via `0784975aa1` ("hw/misc: model BCM2835 PM
watchdog countdown", 2026-08-22), which is fork-local. This is a third fork
fidelity improvement over upstream, alongside the H4f HPPIR fix and the 54 MHz
`CNTFRQ_EL0` the fork reports where upstream `raspi4b` reports 62.5 MHz.

Consequence to remember: **an H6 hardware gate baseline must be captured with
the fork.** Upstream cannot run the watchdog-armed image at all.

## Files

| File | Contents |
| --- | --- |
| `h4f-qemu-pi4-fork.log.b64` | H4f payload on the fork, reaches PASS. |
| `h4f-qemu-upstream-11.0.2.log.b64` | Same ELF on unmodified upstream, fails at the SGI HPPIR checkpoint. |
| `h4i-fork-vs-upstream-comparison.json` | `gicv2-lab.comparison.v1` for the H4i fork/upstream run, `matches: true`. |
