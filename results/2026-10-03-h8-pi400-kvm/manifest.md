# H8 on the Pi 400 under KVM, with a physical GIC-400 reference: 2026-10-03

Status: observation recorded. No defect is claimed and nothing was reported
externally. Board: Raspberry Pi 400 Rev 1.0 (BCM2711, GIC-400).

## What ran

| Step | Kernel | Result |
| --- | --- | --- |
| Frozen H8 bundle (`h8-qemu-campaign.tar.xz`, SHA-256 `ea9fbac6...43f79`), 20 KVM captures per workload | vendor `6.18.39+rpt-rpi-v8` | 0/60 pass |
| Same bundle, 10 captures per workload | `7.3.0-rc3-p13rtguardfinal-00511-gd4e2f1e3e415` (PREEMPT_RT, 7.3-rc3 plus three locking/mm patches; vgic untouched) | 0/30 pass |
| Edge-triggered diagnostic (one-line change, `edge-diagnostic/`) | both kernels | 60/60 and 30/30 pass and match QEMU |
| Observe-only diagnostic (records every mismatch, `observe-diagnostic/`) | both kernels | identical traces on both kernels, 5 runs per workload |
| `kvm-unit-tests` gic group (`f20b0b8`) | 7.3-rc3 | gicv2-ipi, -mmio, -mmio-up, -mmio-3p, -active all pass |
| Physical GIC-400 probe modules (`physical-gic400-probe/`) | vendor kernel, non-secure view | see below |

The prerequisite `gicv2-mmio-up` result was regenerated under the 7.3-rc3
kernel before its captures. The diagnostic bundles are not frozen lab
contracts; they were built from the dirty tree with the single change recorded
in each directory and have no 20-run QEMU baseline of their own (the
observe-only build has none at all, because it never prints PASS).

## Observation

For a level-sensitive SPI made pending by a GICD_ISPENDR write, with its
input line deasserted:

| Checkpoint | Contract and QEMU | Physical GIC-400 | KVM (both kernels) |
| --- | --- | --- | --- |
| ISPENDR bit after acknowledge | clear | clear | set |
| ISPENDR bit after EOIR, before DIR | clear | clear | set |
| Second ISPENDR write while active, then EOIR and DIR | pending again, HPPIR 42, second IAR 42 | pending again, second acknowledge | pending clear, HPPIR and IAR 1023: no second delivery |

With the SPI configured edge-triggered, KVM matches the contract at every
checkpoint.

## Specification

ARM IHI 0048B.b, Table 4-11 (GICD_ISPENDR writes, level sensitive): a write
changes an active interrupt to active and pending. Figure 4-10 (logic of the
pending status of a level-sensitive interrupt): the latch set by a
GICD_ISPENDR write is cleared by a GICD_ICPENDR write or by a read of GICC_IAR
that acknowledges the interrupt.

GICv3 (ARM IHI 0069H.b) is looser on the first row. Section 4.5 says it is
IMPLEMENTATION DEFINED whether acknowledging a level-sensitive interrupt that
was set pending by a GICD_ISPENDR<n> write clears the pending state. The
GICD_ISPENDR<n> and GICR_ISPENDR0 descriptions still say a write changes an
active interrupt to active and pending, so the lost second pend matches
neither permitted variant. No GICv3 system was tested.

## Mechanism in Linux (read at 50d05c7c76c9, v7.3-rc3 merge window tree)

- `vgic_v2_populate_lr()` clears `pending_latch` only for edge interrupts, and
  `__read_pending()` returns `irq_is_pending()`, so the bit reads set while
  the interrupt is active.
- `vgic_v2_compute_lr()` refuses pending-and-active for level interrupts
  ("Software resampling doesn't work very well if we allow P+A"), from
  67b5b673ad4d (2018, "Disallow Active+Pending for level interrupts").
- `vgic_v2_fold_lr()` clears `pending_latch` for a level interrupt once the
  list register has no state ("Clear soft pending state when level irqs have
  been acked"), which also discards a write made while it was active.

## Limits

- GICv2 only; the equivalent vgic-v3 path was not exercised (the Pi 400 has no
  GICv3).
- The 7.3-rc3 kernel is a patched PREEMPT_RT tree, not pristine mainline.
- The physical probe ran from Linux's non-secure view with interrupts masked
  on one CPU, on INTID 255 at priority 0x40; it is not the lab's EL1 guest.
- Duplicate search: `pending_latch`, `ISPENDR`, `soft pending`,
  `Software resampling` over the newest 20000 kvmarm messages (back to
  2025-04-23) found refactoring patches only. Older archives and other lists
  were not searched.
- Practical impact is unassessed. Linux guests do not resend level interrupts
  through GICD_ISPENDR.

## Physical GIC-400 versus the qemu-pi4 raspi400 model

`physical-gic400-probe/qtest-replay-raspi400.py` replays both probe sequences
through qtest. On INTID 223 the four acknowledge/EOI/DIR/re-pend sequences
match the hardware; 151 of 170 compared values agree. Differences:

- GICD_TYPER 0xfc67 on hardware (256 lines, Security Extensions) against 0x66
  (224 lines, none); INTID 255 does not exist in the model.
- GICD_IIDR 0x0200143b and GICC_IIDR 0x0202143b against 0x43b and 0x2043b.
- Non-secure view on hardware: four priority bits (PMR and IPRIORITYR read
  back 0xf0), minimum BPR 3, ABPR 0, APR0 0x10 for priority 0x40. The model
  shows eight bits, BPR 0, ABPR 1, APR0 0.
- GICD_ICFGR1 is 0x55540000 and read-only on hardware; 0 and writable in the
  model. SPI configuration bit 0 reads as one on hardware.
- GICC_CTLR keeps bits 5 and 6 on hardware (0x261); the model returns 0x201.
- GICD_ITARGETSR accepts four CPU bits on hardware, eight in the model.
- With an interrupt masked only by PMR, hardware GICC_HPPIR still reports its
  ID; the model reports 1023.

After this comparison the fork's board model was changed (256 interrupt IDs,
Security Extensions reset to Group 1, five priority bits, captured IIDR and
TYPER values; uncommitted in `qemu-pi4` as of 2026-10-03). The rebuilt model
matches 168 of 170 values on INTID 255 after a second round that added the
GIC-400's PPI set, configuration bits, CTLR bypass bits, target width, masked
HPPIR and active-priority layout; the two left only record which CPU ran the
probe. The lab monitor's GIC register lines equal the board's H6 trace. `scripts/smoke-qemu.sh` now expects
`GICC_PMR_enabled=0xf0`, the hardware value, and needs that rebuilt model.

## Files

`SHA256SUMS` covers every file here. Raw traces are inside the `*.tar.xz`
archives; `remote-*.sh` are the exact commands run on the board.
