# H4c virtual priority and preemption

## Architectural contract

H4c isolates GICv2 virtual priority preemption. A lower-priority virtual
interrupt must already be active at EL1 before EL2 injects a second virtual
interrupt whose group priority is strictly higher. EL1 then deliberately
unmasks IRQs inside the first handler and must take exactly one nested IRQ.
The higher-priority interrupt is completed first, followed by the interrupted
lower-priority interrupt.

The H1 and H2 preconditions remain in force. H4c additionally requires at
least two implemented List Registers and the five priority bits reported by
the qemu-pi4 GICH_VTR value. EL2 programs the virtual interface as follows:

- `GICH_HCR=0x1`: virtualization enabled, with every maintenance-enable bit
  clear;
- `GICV_CTLR=0x1`: Group 0 enabled and `EOImode=0`;
- `GICV_PMR=0xf8`;
- `GICV_BPR=2`, the minimum binary point for five implemented priority bits;
- LR0 contains software INTID 42 at priority `0x80`; and
- LR1 later contains software INTID 43 at priority `0x20`.

Both LRs have `HW=0` and `EOI=0`. With BPR 2, the implemented group-priority
fields are `0x80` and `0x20`, respectively. Because a lower numerical value
is higher priority, INTID 43 is at a strictly higher preemption level and can
preempt active INTID 42. The corresponding `GICH_VMCR` value is
`0xf85c0001`.

The software-LR encodings are:

| Interrupt | pending | active | invalid after EOI | APR bit |
| --- | ---: | ---: | ---: | ---: |
| low, INTID 42, priority `0x80` | `0x1800002a` | `0x2800002a` | `0x0800002a` | 16 (`0x00010000`) |
| high, INTID 43, priority `0x20` | `0x1200002b` | `0x2200002b` | `0x0200002b` | 4 (`0x00000010`) |

After acknowledging INTID 42, EL1 calls `HVC_IRQ_ACTIVE` while IRQs remain
masked by exception entry. EL2 first requires LR0 active and APR bit 16 set,
then writes the pending high-priority interrupt to LR1. After returning, the
low handler changes its software depth from zero to one and clears PSTATE.I.
The high interrupt must enter the current-EL SPx IRQ vector at depth one.

Because the nested exception overwrites `ELR_EL1` and `SPSR_EL1`, every guest
vector entry saves both registers before the low handler is allowed to
unmask IRQs. The nested handler acknowledges INTID 43, reports the
both-active checkpoint, EOIs INTID 43, reports the high-EOI checkpoint, and
returns to the low handler. The low handler then masks IRQs, EOIs INTID 42,
and reports the final completion. Any other depth, vector, INTID, order, or
event count is forbidden.

The eight required snapshots are:

| Checkpoint | LR0 | LR1 | APR | ELRSR0 with 4 LRs |
| --- | ---: | ---: | ---: | ---: |
| initialized | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |
| low pending | `0x1800002a` | `0x00000000` | `0x00000000` | `0xe` |
| low active | `0x2800002a` | `0x00000000` | `0x00010000` | `0xe` |
| high pending while low active | `0x2800002a` | `0x1200002b` | `0x00010000` | `0xc` |
| both active | `0x2800002a` | `0x2200002b` | `0x00010010` | `0xc` |
| high EOI | `0x2800002a` | `0x0200002b` | `0x00010000` | `0xe` |
| low EOI | `0x0800002a` | `0x0200002b` | `0x00000000` | `0xf` |
| EL2 clear | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |

At every checkpoint, `GICH_HCR` must remain `0x1`, `GICH_VMCR` must remain
`0xf85c0001`, and MISR and both EISR banks must be zero. `EISR1` and
`ELRSR1` are zero for the four-LR qemu-pi4 model. The implementation derives
the meaningful ELRSR masks from the discovered LR count.

An interrupt before the low handler explicitly unmasks IRQs, failure to
preempt, a second nested interrupt, acknowledgement in any order other than
42 then 43, EOI in any order other than 43 then 42, a maintenance cause or
physical IRQ, a changed VMCR, any other LR/APR/MISR/EISR/ELRSR value, a
duplicate HVC, unexpected exception, reset, hang, or early exit is forbidden.
The strict oracle is `scripts/smoke-qemu.sh` at the H4c implementation
revision recorded by the result manifest.

The architectural rules and register encodings are defined by the [Arm
Generic Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/), particularly
the virtual CPU-interface priority rules and GICH_VMCR, GICH_APR, GICH_LR,
GICV_IAR, and GICV_EOIR.

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict oracle, with no forbidden outcome. The tested source revision, image
and QEMU hashes, toolchain, host, exact command, and lossless final serial
trace must be preserved under `results/` before H4c is marked complete.

## Recorded result

The strict oracle passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22 with no forbidden outcome. The exact tested lab revision, artifact
and QEMU binary hashes, toolchain, command line, host environment, linked
vector audit, final raw serial trace, and QEMU cleanup diagnostic are recorded
in `../results/2026-08-22-h4c-qemu-pi4/manifest.md`.
