# H4e active-plus-pending redelivery

## Architectural contract

H4e isolates redelivery of one software-originated virtual interrupt through
the GICv2 List Register `Pending+Active` state. It models a virtual level
source that remains asserted, or is asserted again, while its first delivery
is active. Because a GICH_LR has no edge/level configuration bit, the exact
architectural subject of this test is the hypervisor-managed LR state machine,
not sampling of a physical device signal.

The H1 and H2 preconditions remain in force. H4e requires at least two
implemented List Registers so LR1 can be observed remaining empty, and the
five priority/preemption bits reported by qemu-pi4. EL2 retains H4d's virtual
interface configuration:

- `GICH_HCR=0x1`, with every maintenance-enable bit and `EOICount` clear;
- `GICV_CTLR=0x201`, enabling Group 0 and split EOI mode;
- `GICV_PMR=0xf8` and `GICV_BPR=2`;
- one software-originated (`HW=0`, `EOI=0`) INTID 42 at priority `0x80` in
  LR0; and
- LR1 zero throughout the scenario.

`GICH_VMCR` is therefore `0xf85c0201`. LR0's invalid, pending, active, and
Pending+Active encodings are respectively `0x0800002a`, `0x1800002a`,
`0x2800002a`, and `0x3800002a`. At BPR 2, the corresponding active-priority
bit is GICH_APR bit 16 (`0x00010000`).

The required sequence is:

1. EL2 initializes an empty virtual interface and injects pending INTID 42.
2. EL1 acknowledges the first delivery through `GICV_IAR`. LR0 becomes
   active, APR bit 16 is set, RPR is `0x80`, and HPPIR is the spurious INTID
   1023.
3. During the first active report, EL2 validates that state and rewrites only
   LR0's State field from Active to Pending+Active. This is the one deliberate
   re-pend operation in the scenario.
4. EL1 writes its first raw IAR value to `GICV_EOIR`. RPR becomes `0xff` and
   APR clears, but LR0 remains Pending+Active. `GICV_HPPIR` must still return
   1023 because an Active+Pending interrupt is not eligible for signaling.
5. EL1 writes the same value to `GICV_DIR`. Deactivation changes LR0 from
   Pending+Active to Pending. RPR remains `0xff`, HPPIR now returns INTID 42,
   and EL2 leaves the pending LR intact.
6. The first handler returns, restoring the unmasked EL1 state. EL1 must take
   exactly one second IRQ and acknowledge INTID 42 again. LR0 becomes Active,
   APR bit 16 is set again, RPR is `0x80`, and HPPIR returns 1023.
7. The second EOIR performs only priority drop, leaving LR0 Active with APR
   clear and RPR `0xff`. The second DIR changes LR0 from Active to Invalid.
8. EL2 validates the final deactivation, clears LR0 to zero, and accepts EXIT
   only after both deliveries completed in order.

The ten required hypervisor snapshots are:

| Checkpoint | LR0 | LR1 | APR | ELRSR0 with 4 LRs |
| --- | ---: | ---: | ---: | ---: |
| initialized | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |
| initially pending | `0x1800002a` | `0x00000000` | `0x00000000` | `0xe` |
| first active | `0x2800002a` | `0x00000000` | `0x00010000` | `0xe` |
| re-pended Active+Pending | `0x3800002a` | `0x00000000` | `0x00010000` | `0xe` |
| first priority drop | `0x3800002a` | `0x00000000` | `0x00000000` | `0xe` |
| first deactivation, pending again | `0x1800002a` | `0x00000000` | `0x00000000` | `0xe` |
| second active | `0x2800002a` | `0x00000000` | `0x00010000` | `0xe` |
| second priority drop | `0x2800002a` | `0x00000000` | `0x00000000` | `0xe` |
| final deactivation | `0x0800002a` | `0x00000000` | `0x00000000` | `0xf` |
| EL2 clear | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |

At every checkpoint, HCR must remain `0x1`, VMCR must remain `0xf85c0201`,
and MISR and both EISR banks must be zero. `EISR1` and `ELRSR1` are zero for
the four-LR qemu-pi4 model. The implementation derives meaningful ELRSR masks
from the discovered LR count.

Each interrupt-state HVC carries the raw IAR value in bits 31:0, RPR in bits
39:32, and the low ten HPPIR bits in bits 49:40. The required `(IAR, RPR,
HPPIR)` triples are `(42, 0x80, 1023)` for both active reports, `(42, 0xff,
1023)` after both EOIR writes, `(42, 0xff, 42)` after the first DIR, and `(42,
0xff, 1023)` after the final DIR.

Returning anything other than spurious from HPPIR for the Active+Pending
entry, failing to expose or deliver the resulting Pending entry, any IAR other
than 42, more or fewer than two deliveries, another EL2 re-pend, a changed or
prematurely empty LR, a nonzero EOICount, any maintenance cause or physical
IRQ, a changed VMCR, any other LR/APR/MISR/EISR/ELRSR/RPR/HPPIR value,
duplicate or reordered HVC, unexpected exception, reset, hang, or early exit
is forbidden. The strict oracle is `scripts/smoke-qemu.sh` at the H4e
implementation revision recorded by the result manifest.

The architectural rules and register encodings are defined by the [Arm
Generic Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/), particularly the
interrupt state machine, GICH_LR, GICV_HPPIR, GICV_EOIR, and GICV_DIR.

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict oracle, with no forbidden outcome. The tested source revision, image
and QEMU hashes, toolchain, host, exact command, linked-image audit, and
lossless final serial trace must be preserved under `results/` before H4e is
marked complete.
