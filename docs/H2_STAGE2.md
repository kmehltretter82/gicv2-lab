# H2 stage-2 translation scenario

## Architectural contract

H2 tests one narrow rule: when stage-2 translation is enabled for an AArch64
EL1 access and the final required stage-2 descriptor is invalid, the access
takes a synchronous Data Abort to EL2 with a translation-fault status for the
level at which the walk failed.

This scenario has these fixed preconditions:

- execution starts in non-secure AArch64 EL2 on vCPU 0;
- `HCR_EL2.VM=1` and `HCR_EL2.RW=1`;
- the guest runs at EL1h with its stage-1 MMU, data cache, and instruction
  cache disabled;
- stage 2 uses a 4 KiB granule, a 32-bit IPA, and an L1 root table;
- L1 entry 0 points to an L2 table;
- L2 entry 1 maps IPA `0x00200000` to the same PA as one normal,
  inner-shareable, read/write/executable 2 MiB block; and
- L2 entry 0 remains invalid, so IPA `0x00080000` is not mapped.

The guest first proves it reached EL1 through REPORT. It then performs one
64-bit load from IPA `0x00080000`, the monitor's own load address. The expected
trap uses lower-EL AArch64 vector slot 8, Data Abort EC `0x24`, FAR
`0x00080000`, and level-2 translation-fault FSC `0x06`. EL2 advances
`ELR_EL2` by exactly four bytes for this known instruction and resumes the
guest. Reaching PASS and EXIT proves forward progress after the fault.

A missing trap, a different EC/FSC/FAR, a duplicate abort, or failure to reach
PASS is forbidden by this test. This expected outcome does not rely on an
IMPLEMENTATION DEFINED choice.

## Fixed memory map

H3 preserves the H2 guest mapping and fault target, and adds the minimum MMIO
mapping needed for the virtual interrupt scenario. The current tables expose
no other peripheral page to EL1.

| Address range | Owner | Current stage-2 view |
| --- | --- | --- |
| `0x00080000` to `__monitor_end` | EL2 text, data, tables, and 16 KiB stack | deliberately unmapped |
| `0x00200000` to `0x003fffff` | EL1 guest 2 MiB block | identity-mapped normal RWX memory |
| `0x00200000` | H2 guest payload | guest entry point |
| `0x003ff000` | initial `SP_EL1` | stack grows downward if used |
| `0xff846000` to `0xff846fff` | GICV virtual CPU-interface frame | identity-mapped Device-nGnRE, inner-shareable, RW, XN page |

The GICV page descriptor is `0x00400000ff8467c7` in the qemu-pi4 smoke
configuration. GICD, GICC, GICH, PL011, and the rest of the GICV surrounding
region remain unmapped at stage 2.

RWX is intentional for this first single-block experiment, not a security
model. A later scenario can split code and data permissions when that
distinction becomes part of the test.

The linked monitor is asserted to end before the guest block, and the guest
payload is asserted not to overlap its stack. Stage-2 translation applies to
EL0/EL1 accesses; the EL2 monitor continues to access its own RAM, PL011, and
physical GIC frames directly.

Normative architecture reference: [Arm Architecture Reference Manual for
A-profile architecture, 4 KiB stage-2 translation](https://developer.arm.com/documentation/ddi0487/mc/-Part-D-The-AArch64-System-Level-Architecture/-Chapter-D8-The-AArch64-Virtual-Memory-System-Architecture/-D8-2-Translation-process/-D8-2-8-VMSAv8-64-translation-using-the-4KB-granule?lang=en).
The [Armv8-A virtualization guide](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Armv8-A%20virtualization.pdf?revision=a765a7df-1a00-434d-b241-357bfda2dd31)
provides an overview of the EL2 and stage-2 controls.
