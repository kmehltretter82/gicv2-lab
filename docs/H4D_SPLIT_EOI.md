# H4d split priority drop and deactivation

## Architectural contract

H4d isolates the GICv2 split-EOI state machine for one software virtual
interrupt. With `GICV_CTLR.EOImode=1`, a valid write to `GICV_EOIR` performs
only the priority-drop operation. The interrupt remains active in its List
Register until a later valid write to `GICV_DIR` performs deactivation.

The H1 and H2 preconditions remain in force. H4d additionally requires at
least two implemented List Registers so that LR1 can be observed remaining
empty, and the five priority/preemption bits reported by qemu-pi4. EL2
programs the virtual interface as follows:

- `GICH_HCR=0x1`: virtualization enabled, with every maintenance-enable bit
  clear and `EOICount=0`;
- `GICV_CTLR=0x201`: Group 0 and split EOI mode enabled;
- `GICV_PMR=0xf8`;
- `GICV_BPR=2`;
- LR0 contains software INTID 42 at priority `0x80`; and
- LR1 remains zero throughout the scenario.

LR0 has `HW=0` and `EOI=0`, so deactivation is entirely virtual and cannot
request EOI maintenance. The corresponding `GICH_VMCR` value is
`0xf85c0201`. LR0's pending, active, and invalid encodings are respectively
`0x1800002a`, `0x2800002a`, and `0x0800002a`. At BPR 2 its active priority is
represented by GICH_APR bit 16 (`0x00010000`).

The required sequence is:

1. EL2 initializes an empty interface and injects pending INTID 42 into LR0.
2. EL1 acknowledges it through `GICV_IAR`. LR0 becomes active, APR bit 16 is
   set, and `GICV_RPR` reads `0x80`.
3. EL1 writes the raw IAR value to `GICV_EOIR`. APR clears and `GICV_RPR`
   becomes the idle priority `0xff`, but LR0 must remain active and nonempty.
4. Only after EL2 validates that priority-drop checkpoint does EL1 write the
   same raw IAR value to `GICV_DIR`.
5. LR0 changes from active to invalid while retaining its priority and INTID
   fields. EL2 validates that state and then clears LR0 to zero.

`GICV_DIR` is at offset `0x1000` in the virtual CPU-interface frame. H4d
therefore expands the guest's stage-2 Device-nGnRE mapping from one 4 KiB
page to exactly the 8 KiB architectural GICV register block:

| IPA/PA | Purpose | Stage-2 descriptor |
| ---: | --- | ---: |
| `0xff846000` | GICV control, IAR, EOIR, RPR | `0x00400000ff8467c7` |
| `0xff847000` | page containing GICV_DIR | `0x00400000ff8477c7` |

No other peripheral or surrounding MMIO page becomes guest-accessible.

The six required hypervisor snapshots are:

| Checkpoint | LR0 | LR1 | APR | ELRSR0 with 4 LRs |
| --- | ---: | ---: | ---: | ---: |
| initialized | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |
| pending | `0x1800002a` | `0x00000000` | `0x00000000` | `0xe` |
| active | `0x2800002a` | `0x00000000` | `0x00010000` | `0xe` |
| priority dropped by EOIR | `0x2800002a` | `0x00000000` | `0x00000000` | `0xe` |
| deactivated by DIR | `0x0800002a` | `0x00000000` | `0x00000000` | `0xf` |
| EL2 clear | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |

At every checkpoint, `GICH_HCR` must remain `0x1`, `GICH_VMCR` must remain
`0xf85c0201`, and MISR and both EISR banks must be zero. `EISR1` and
`ELRSR1` are zero for the four-LR qemu-pi4 model. The implementation derives
the meaningful ELRSR masks from the discovered LR count.

The EL1 active, priority-drop, and deactivation reports must carry the exact
raw IAR value 42 and the respective `GICV_RPR` values `0x80`, `0xff`, and
`0xff`. A DIR before a successful EOIR, combined EOI/deactivation, a changed
or empty LR at the priority-drop checkpoint, a still-active LR after DIR,
EOICount increment, any maintenance cause or physical IRQ, any other
LR/APR/MISR/EISR/ELRSR value, duplicate HVC, unexpected exception, reset,
hang, or early exit is forbidden. The strict oracle is
`scripts/smoke-qemu.sh` at the H4d implementation revision recorded by the
result manifest.

The architectural rules and register encodings are defined by the [Arm
Generic Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/), particularly
GICV_CTLR, GICV_EOIR, GICV_RPR, GICV_DIR, GICH_APR, and GICH_LR.

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict oracle, with no forbidden outcome. The tested source revision, image
and QEMU hashes, toolchain, host, exact command, and lossless final serial
trace must be preserved under `results/` before H4d is marked complete.
