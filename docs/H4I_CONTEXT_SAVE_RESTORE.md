# H4i paused virtual-interface save and restore

## Architectural contract

H4i isolates one vCPU virtual-interface context switch. EL2 pauses while a
lower-priority virtual interrupt is active and a higher-priority virtual
interrupt is pending, saves every writable GICv2 virtualization-interface
register used by this implementation, installs a distinct quiescent context,
and restores the saved state with `GICH_HCR` enabled last. The guest must then
continue the interrupted handler, take the restored pending interrupt as a
nested IRQ, and complete both interrupts in reverse priority order.

The scenario requires the four LRs and five virtual priority bits reported by
qemu-pi4's `GICH_VTR=0x90000003`. The initial virtual CPU interface has:

- `GICH_HCR=0x1` and `GICH_VMCR=0xf85c0201`;
- split EOI mode, virtual Group 0 enabled, PMR `0xf8`, and BPR 2;
- zero APR and LRs, `GICH_ELRSR0=0xf`, and zero MISR/EISR; and
- every PPI and every maintenance cause disabled.

H4i uses two software-originated (`HW=0`, `EOI=0`) Group 0 virtual
interrupts:

| Role | INTID | Priority | Pending | Active | Invalid |
| --- | ---: | ---: | ---: | ---: | ---: |
| lower | 50 | `0x80` | `0x18000032` | `0x28000032` | `0x08000032` |
| higher | 51 | `0x20` | `0x12000033` | `0x22000033` | `0x02000033` |

The required sequence is:

1. EL2 injects lower INTID 50 into LR0. EL1 unmasks IRQs, acknowledges it,
   and reports LR0 Active with APR bit 16 set.
2. During that report, EL2 installs higher INTID 51 Pending in LR1. It
   captures the complete state and copies the writable vCPU context:
   `GICH_HCR`, `GICH_VMCR`, `GICH_APR`, and all four implemented LRs.
3. EL2 writes `GICH_HCR=0` first. VMCR, APR, and every LR must remain equal
   to the saved values while the interface is disabled.
4. With HCR still zero, EL2 clears all LRs and APR and writes zero to VMCR.
   Because this implementation exposes five priority bits, VMCR's virtual
   BPR and ABPR fields have minimum readback values 2 and 3. The required
   quiescent readback is therefore `GICH_VMCR=0x004c0000`, not literal zero.
   All LRs and APR are zero and `GICH_ELRSR0=0xf`.
5. EL2 restores VMCR, APR, and all four LRs while HCR remains zero. It checks
   the exact saved payload, then writes the saved HCR as the final operation.
   The resulting complete snapshot must equal the original saved snapshot,
   including derived MISR, EISR, and ELRSR values.
6. EL1 records handler depth one and unmasks IRQs. Higher INTID 51 must
   preempt, become Active alongside INTID 50, and set APR bits 4 and 16.
7. The nested handler writes INTID 51 to EOIR, dropping its priority while
   both LRs remain Active, then writes it to DIR. LR1 becomes Invalid and the
   lower handler resumes with LR0 still Active and APR bit 16 set.
8. The lower handler writes INTID 50 to EOIR and DIR. APR becomes zero, both
   LRs become Invalid, and EL2 clears the full LR bank before accepting EXIT.

The exact hypervisor-interface state progression is:

| Checkpoint | HCR | VMCR | LR0 | LR1 | APR | ELRSR0 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| lower pending | `1` | `f85c0201` | `18000032` | `0` | `0` | `e` |
| lower active | `1` | `f85c0201` | `28000032` | `0` | `10000` | `e` |
| context saved | `1` | `f85c0201` | `28000032` | `12000033` | `10000` | `c` |
| HCR disabled | `0` | `f85c0201` | `28000032` | `12000033` | `10000` | `c` |
| quiescent | `0` | `004c0000` | `0` | `0` | `0` | `f` |
| payload restored | `0` | `f85c0201` | `28000032` | `12000033` | `10000` | `c` |
| HCR restored | `1` | `f85c0201` | `28000032` | `12000033` | `10000` | `c` |
| both active | `1` | `f85c0201` | `28000032` | `22000033` | `10010` | `c` |
| higher priority dropped | `1` | `f85c0201` | `28000032` | `22000033` | `10000` | `c` |
| higher deactivated | `1` | `f85c0201` | `28000032` | `02000033` | `10000` | `e` |
| lower priority dropped | `1` | `f85c0201` | `28000032` | `02000033` | `0` | `e` |
| both deactivated | `1` | `f85c0201` | `08000032` | `02000033` | `0` | `f` |
| cleared | `1` | `f85c0201` | `0` | `0` | `0` | `f` |

LR2 and LR3 are zero at every checkpoint. MISR and both EISR banks are also
zero throughout. The guest-visible checkpoints require:

| Checkpoint | IAR | RPR | HPPIR |
| --- | ---: | ---: | ---: |
| lower active | 50 | `0x80` | 1023 |
| higher active | 51 | `0x20` | 1023 |
| higher priority dropped | 51 | `0x80` | 1023 |
| higher deactivated | 51 | `0x80` | 1023 |
| lower resumed | 50 | `0x80` | 1023 |
| lower priority dropped | 50 | `0xff` | 1023 |
| lower deactivated | 50 | `0xff` | 1023 |

The strict oracle rejects a duplicate or reordered HVC, a different saved or
restored bit, enabling HCR before the payload is restored, an incorrect
nested-delivery order, an extra or missing acknowledgement, any maintenance
cause or physical IRQ, an unexpected exception, reset, hang, or early exit.

This is a focused in-place vCPU context-switch test. It does not test QEMU
migration, QMP state transfer, SMP scheduling, or host concurrency.

The controlling virtual-interface, List Register, active-priority, binary
point, and split-EOI rules are described by the
[Arm Generic Interrupt Controller Architecture Specification, version 2](https://developer.arm.com/documentation/ihi0048/latest/).

## Exit gate

Freeze the implementation first, then require 100 consecutive fresh
qemu-pi4 processes accepted by `scripts/smoke-qemu.sh`. Preserve the exact
source revision, image and emulator hashes, toolchain, host, command line,
linked-image audit, and lossless final trace under `results/` before marking
H4i complete.

## Recorded QEMU result

The frozen implementation at `0e4374701ff73860a300e20ba76c75d45cf14d04`
passed 100 consecutive fresh qemu-pi4 processes on 2026-08-22. The exact
image, emulator, environment, linked-image audit, and lossless final trace are
recorded in `results/2026-08-22-h4i-qemu-pi4/manifest.md`.

The observed result matched this contract. H4i produced no divergent
behavior and no QEMU bug candidate. A pre-freeze literal-zero VMCR readback
expectation was corrected to the architecturally canonical minimum BPR/ABPR
readback before the implementation was frozen.
