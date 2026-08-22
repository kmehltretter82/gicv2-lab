# H4b underflow maintenance

## Architectural contract

H4b isolates the GICv2 underflow maintenance condition. The architecture
asserts `GICH_MISR.U` when `GICH_HCR.UIE=1` and zero or one List Register has
a valid interrupt, meaning its State field is not invalid. The maintenance
interrupt is asserted only while `GICH_HCR.En=1` and at least one MISR cause
is present.

The H1 and H2 preconditions remain in force. H4b additionally requires at
least two implemented LRs. The qemu-pi4 machine currently reports four. EL2
initializes every LR and APR to zero with `GICH_HCR=0x1`, then writes two
distinct pending software interrupts while UIE is disabled:

- LR0 holds INTID 42 at priority `0x20`, encoded as `0x1200002a`;
- LR1 holds INTID 43 at priority `0xf8`, encoded as `0x1f80002b`; and
- both LRs have `HW=0` and `EOI=0`, so neither can request EOI maintenance.

The virtual priority mask remains `0xf8`. LR1's priority is equal to that
mask, so LR1 remains valid and pending but is not signaled to EL1. After both
LRs are valid, EL2 sets UIE, changing `GICH_HCR` from `0x1` to `0x3`. Two
valid LRs do not satisfy the underflow condition, so MISR must remain zero.

The guest acknowledges INTID 42 and executes `HVC_IRQ_ACTIVE` before writing
`GICV_EOIR`. This preserves H4a's active-state observation. With
`GICV_CTLR.EOImode=0`, the EOI changes LR0 from active to invalid and clears
APR bit 0. LR1 remains pending, leaving exactly one valid LR and therefore
asserting only `GICH_MISR.U`.

The five required snapshots are:

| Checkpoint | HCR | LR0 | LR1 | APR | MISR | EISR0 | ELRSR0 with 4 LRs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| initialized | `0x1` | `0x00000000` | `0x00000000` | `0x0` | `0x0` | `0x0` | `0xf` |
| armed | `0x3` | `0x1200002a` | `0x1f80002b` | `0x0` | `0x0` | `0x0` | `0xc` |
| acknowledged | `0x3` | `0x2200002a` | `0x1f80002b` | `0x1` | `0x0` | `0x0` | `0xc` |
| underflow | `0x3` | `0x0200002a` | `0x1f80002b` | `0x0` | `0x2` | `0x0` | `0xd` |
| EL2 clear | `0x1` | `0x00000000` | `0x00000000` | `0x0` | `0x0` | `0x0` | `0xf` |

At every checkpoint, `GICH_VMCR` must remain `0xf8fc0001`. `EISR1` and
`ELRSR1` are zero for the four-LR qemu-pi4 model. The implementation derives
the meaningful ELRSR masks from the discovered LR count.

EL2 must observe physical maintenance PPI 25, the exact underflow snapshot,
and no EOI status. It then disables UIE before clearing LR0 and LR1, EOIs PPI
25, and requires a fully empty, quiescent virtual interface. Disabling UIE is
what removes the level-sensitive underflow cause; merely clearing the LRs
would still leave zero valid entries and therefore keep underflow asserted.

The physical maintenance PPI can arrive before or after the guest's later
`HVC_IRQ_EOI` report. Both orderings are permitted, but the active checkpoint
must precede the guest EOI and both observations must occur exactly once
before EXIT.

An early maintenance PPI while two LRs are valid, a second guest interrupt,
any EOI/EISR cause, a wrong IAR, a changed VMCR, any other LR/APR/MISR/ELRSR
value, a duplicate event, unexpected exception, reset, hang, or early exit is
forbidden. The strict oracle is `scripts/smoke-qemu.sh`.

The architectural rule and register encodings are defined by the [Arm Generic
Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/), particularly
GICH_HCR, GICH_MISR, GICH_ELRSR, GICH_LR, and GICV_EOIR.

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict oracle, with no forbidden outcome. The tested source revision, image
and QEMU hashes, toolchain, host, exact command, and lossless final serial
trace must be preserved under `results/` before H4b is marked complete.
