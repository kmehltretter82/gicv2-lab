# H4g List Register exhaustion and software refill

## Architectural contract

H4g isolates the boundary between the finite GICv2 List Register bank and a
hypervisor's software queue. The GIC architecture provides LRs for resident
virtual interrupts and explicitly permits a hypervisor to keep additional
pending interrupts in memory, then move them into an empty LR. The
architecture does not prescribe the queue policy or exact refill point. Those
are deliberately frozen here as gicv2-lab policy so the resulting GIC state
transitions are deterministic.

This scenario requires exactly four implemented LRs, as reported by
qemu-pi4's `GICH_VTR`. EL2 creates five unique software-originated (`HW=0`,
`EOI=0`) Group 0 virtual interrupts:

| Delivery | INTID | Priority | Initial location | Pending | Active | Invalid |
| ---: | ---: | ---: | --- | ---: | ---: | ---: |
| 0 | 40 | `0x20` | LR0 | `0x12000028` | `0x22000028` | `0x02000028` |
| 1 | 41 | `0x40` | LR1 | `0x14000029` | `0x24000029` | `0x04000029` |
| 2 | 42 | `0x60` | LR2 | `0x1600002a` | `0x2600002a` | `0x0600002a` |
| 3 | 43 | `0x80` | LR3 | `0x1800002b` | `0x2800002b` | `0x0800002b` |
| 4 | 44 | `0xa0` | EL2 memory, then LR0 | `0x1a00002c` | `0x2a00002c` | `0x0a00002c` |

The virtual interface remains `GICH_HCR=0x1`, `GICH_VMCR=0xf85c0201`,
`GICV_CTLR=0x201`, `GICV_PMR=0xf8`, and `GICV_BPR=2`. All maintenance enables
and EOICount are clear, every PPI is disabled, and any physical IRQ is
forbidden.

The required sequence is:

1. EL2 fills LR0-LR3 with INTIDs 40-43 and stores pending encoding
   `0x1a00002c` only in its software queue. All four ELRSR0 bits are clear;
   the queue entry must not affect HPPIR, which returns 40.
2. EL1 acknowledges INTID 40. LR0 becomes Active, APR is `0x10`, RPR is
   `0x20`, and HPPIR returns 41. EOIR clears APR and changes RPR to `0xff`
   without deactivation. DIR makes LR0 Invalid and sets ELRSR0 bit 0.
3. During the completion HVC, EL2 validates that empty slot and performs the
   scenario's only refill: it writes queued INTID 44 Pending into LR0 and
   clears the software queue. ELRSR0 returns to zero. INTID 41 remains the
   highest-priority pending interrupt.
4. EL1 completes INTIDs 41, 42, and 43 in order. Their invalid LRs remain
   untouched; ELRSR0 therefore progresses through `0x2`, `0x6`, and `0xe`.
5. EL1 acknowledges refilled INTID 44 from LR0. With no other valid pending
   entry, HPPIR is 1023. Its EOIR and DIR leave every LR Invalid and ELRSR0
   `0xf`. EL2 then clears all four LRs to zero.

For delivery `n`, the Active checkpoint requires APR bit
`1 << (priority / 8)`, giving `0x10`, `0x100`, `0x1000`, `0x10000`, and
`0x100000`. Every EOIR checkpoint requires APR zero while the corresponding
LR remains Active. Every DIR checkpoint requires that LR's Invalid encoding.
MISR and both EISR banks remain zero at every checkpoint.

The complete resident-LR progression is:

| Checkpoint | LR0 | LR1 | LR2 | LR3 | APR | ELRSR0 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| all pending | `12000028` | `14000029` | `1600002a` | `1800002b` | `0` | `0` |
| 40 active | `22000028` | `14000029` | `1600002a` | `1800002b` | `10` | `0` |
| 40 dropped | `22000028` | `14000029` | `1600002a` | `1800002b` | `0` | `0` |
| 40 invalid | `02000028` | `14000029` | `1600002a` | `1800002b` | `0` | `1` |
| refilled | `1a00002c` | `14000029` | `1600002a` | `1800002b` | `0` | `0` |
| 41 active | `1a00002c` | `24000029` | `1600002a` | `1800002b` | `100` | `0` |
| 41 invalid | `1a00002c` | `04000029` | `1600002a` | `1800002b` | `0` | `2` |
| 42 active | `1a00002c` | `04000029` | `2600002a` | `1800002b` | `1000` | `2` |
| 42 invalid | `1a00002c` | `04000029` | `0600002a` | `1800002b` | `0` | `6` |
| 43 active | `1a00002c` | `04000029` | `0600002a` | `2800002b` | `10000` | `6` |
| 43 invalid | `1a00002c` | `04000029` | `0600002a` | `0800002b` | `0` | `e` |
| 44 active | `2a00002c` | `04000029` | `0600002a` | `0800002b` | `100000` | `e` |
| all invalid | `0a00002c` | `04000029` | `0600002a` | `0800002b` | `0` | `f` |
| cleared | `0` | `0` | `0` | `0` | `0` | `f` |

The strict oracle additionally requires exactly five IAR reads, five EOIR
writes, five DIR writes, the priority order 40-44, next-pending HPPIR values
41-44 followed by 1023, exactly one queue drain and one LR refill, and exactly
one final clear. Duplicate or reordered HVCs, an extra or missing delivery,
premature appearance of INTID 44, a maintenance cause, physical IRQ,
unexpected exception, reset, hang, or early exit is forbidden.

The controlling architectural descriptions are in sections 5.2, 5.2.1,
5.3.8, 5.5.4, 5.5.7, 5.5.14, and 5.5.16 of the
[Arm Generic Interrupt Controller Architecture Specification, version 2](https://developer.arm.com/documentation/ihi0048/latest/).

## Exit gate

Freeze the implementation first, then require 100 consecutive fresh
qemu-pi4 processes accepted by `scripts/smoke-qemu.sh`. Preserve the exact
source revision, image and emulator hashes, toolchain, host, command line,
linked-image audit, and lossless final trace under `results/` before marking
H4g complete.

## Recorded QEMU result

The frozen implementation at `7945ed1352c25a2c5146940100c2b024d51ba8e3`
passed 100 consecutive fresh qemu-pi4 processes on 2026-08-22. The exact
image, emulator, environment, linked-image audit, and lossless final trace are
recorded in `results/2026-08-22-h4g-qemu-pi4/manifest.md`.

The observed result matched this contract. H4g produced no divergent
behavior and no QEMU bug candidate.
