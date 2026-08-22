# H3 first virtual-interrupt scenario

## Architectural contract

H3 exercises one complete GICv2 virtualization path: EL2 places one pending
software virtual interrupt in a List Register, EL1 acknowledges and EOIs it
through the virtual CPU interface, and EL2 services the resulting EOI
maintenance interrupt.

The scenario has these fixed preconditions:

- H1 and H2 have passed on vCPU 0 in non-secure AArch64 EL2;
- `GICH_VTR.ListRegs + 1` is between 1 and 64 and determines how many List
  Registers are initialized and which `EISR`/`ELRSR` bits are meaningful;
- the physical distributor and CPU interface are enabled for the maintenance
  interrupt, PPI 25, at priority `0x20`;
- the virtual interface is enabled with virtual priority mask `0xf8` and both
  virtual binary points set to 7;
- `HCR_EL2.IMO=1` routes the physical maintenance IRQ to EL2;
- the guest's existing 2 MiB RAM block remains identity-mapped; and
- only the 4 KiB GICV frame at `0xff846000` is additionally visible to EL1,
  as Device-nGnRE, inner-shareable, read/write, execute-never memory.

GICD, GICC, and GICH remain inaccessible at EL1. The guest runs with its
stage-1 MMU and caches disabled, as in H2.

## Tested transition

EL2 clears all implemented List Registers and writes LR0 as a pending virtual
INTID 42 at priority `0x20`, with EOI maintenance enabled. For this software
List Register encoding, the injected value is `0x1208002a`.

The EL1 guest installs a complete vector table, verifies that GICV reports
`CTLR=1` and `PMR=0xf8`, reports readiness with `HVC_IRQ_READY`, and unmasks
IRQs. Its current-EL SPx IRQ vector then:

1. reads `GICV_IAR` and requires INTID 42;
2. writes the returned value to `GICV_EOIR`;
3. records that the virtual IRQ was seen; and
4. reports the acknowledgement to EL2 with `HVC_IRQ_EOI`.

The GIC can signal maintenance immediately after the `GICV_EOIR` write, so
the EL2 maintenance handler may run before or after step 4. Both orderings are
valid. The exit gate requires exactly one valid guest report and exactly one
valid maintenance event before H3 can pass.

For the EOI maintenance event, EL2 requires:

- physical `GICC_IAR` INTID 25;
- `GICH_MISR.EOI=1` and only `GICH_EISR` bit 0 set;
- LR0 invalid with its EOI bit retained, value `0x0208002a`;
- `GICH_APR=0`; and
- every other implemented List Register still empty.

EL2 records that state, clears LR0, EOIs physical PPI 25, and then requires
`MISR=0`, `EISR=0`, LR0 zero, and all implemented `ELRSR` bits set. With the
four List Registers currently modeled by qemu-pi4, `ELRSR0` therefore changes
from `0x0000000e` to `0x0000000f`; `EISR1` and `ELRSR1` remain zero because no
high register bank is implemented.

A wrong interrupt ID, unexpected List Register state, missing maintenance
event, duplicate protocol operation, unexpected exception, or early exit is a
forbidden outcome. The guest deliberately spins on memory flags in H3; WFI
wakeup is a separate H4 state-machine scenario and is not a precondition of
this test.

At the recorded H3 source revision
`8890a4d2a41092c1f8feb6dd228682ec54ef4588`, the strict oracle was
`scripts/smoke-qemu.sh`. Later milestones reuse that filename for their current
scenario, so the recorded revision and result manifest pin the H3 oracle.
Register encodings and behavior are defined by the [Arm Generic Interrupt
Controller Architecture
Specification, version 2](https://developer.arm.com/documentation/ihi0048/latest/).
The [Armv8-A virtualization
guide](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Armv8-A%20virtualization.pdf?revision=a765a7df-1a00-434d-b241-357bfda2dd31)
provides the EL2/GIC virtualization overview.

H4a extends this scenario with a synchronous checkpoint between the
`GICV_IAR` read and `GICV_EOIR` write. Its additional LR and APR requirements
are documented in `H4A_LR_LIFECYCLE.md`; they do not alter the recorded H3
result below.

## Recorded result

The strict oracle passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22 with no forbidden outcome. The exact tested lab revision, artifact
and QEMU binary hashes, toolchain, command line, host environment, and final
raw serial trace are recorded in
`results/2026-08-22-h3-qemu-pi4/manifest.md`.
