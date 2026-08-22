# H4f multi-source virtual SGIs

## Architectural contract

H4f isolates the source-CPU field of software-generated virtual interrupts.
The GICv2 architecture requires every valid List Register on one virtual CPU
interface to have a unique VirtualID, so this scenario does not create two
simultaneous entries for the same SGI ID. Instead, it installs two distinct
SGIs carrying two distinct virtual source CPU IDs:

- LR0: SGI 5 from CPUID 1, priority `0x20`;
- LR1: SGI 6 from CPUID 3, priority `0x80`.

Both are software-originated (`HW=0`, `EOI=0`) Group 0 interrupts. Split EOI
mode remains enabled with `GICH_HCR=0x1`, `GICV_CTLR=0x201`, `GICV_PMR=0xf8`,
and `GICV_BPR=2`, so `GICH_VMCR=0xf85c0201`. All maintenance enables and
EOICount remain clear, and every physical IRQ is forbidden.

For a software-originated SGI, GICH_LR bits [12:10] are CPUID and must appear
in the corresponding GICV_HPPIR and GICV_IAR value. EL1 must write the entire
raw IAR value, including CPUID, to EOIR and DIR. The encoded raw values are:

| Interrupt | Pending LR | Active LR | Invalid LR | HPPIR/IAR |
| --- | ---: | ---: | ---: | ---: |
| SGI 5, CPUID 1 | `0x12000405` | `0x22000405` | `0x02000405` | `0x0405` |
| SGI 6, CPUID 3 | `0x18000c06` | `0x28000c06` | `0x08000c06` | `0x0c06` |

The required sequence is:

1. EL2 installs both pending entries while EL1 IRQs are masked. EL1 reads
   HPPIR and must observe raw value `0x0405`, not merely INTID 5.
2. EL1 unmasks IRQs and acknowledges raw IAR `0x0405`. LR0 becomes Active,
   APR bit 4 (`0x10`) is set, RPR is `0x20`, and HPPIR exposes the second raw
   pending value `0x0c06`.
3. EL1 writes raw `0x0405` to EOIR and then DIR. EOIR clears APR without
   deactivating LR0; DIR changes LR0 to Invalid. LR1 and HPPIR remain
   `0x18000c06` and `0x0c06` throughout.
4. After returning from the first handler, EL1 acknowledges raw IAR `0x0c06`.
   LR1 becomes Active, APR bit 16 (`0x10000`) is set, RPR is `0x80`, and HPPIR
   is the spurious value 1023.
5. EL1 writes raw `0x0c06` to EOIR and DIR. EL2 validates both invalid LRs,
   clears them to zero, and accepts EXIT only after exactly two deliveries.

The required hypervisor snapshots are:

| Checkpoint | LR0 | LR1 | APR | ELRSR0 with 4 LRs |
| --- | ---: | ---: | ---: | ---: |
| initialized | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |
| both pending | `0x12000405` | `0x18000c06` | `0x00000000` | `0xc` |
| first active | `0x22000405` | `0x18000c06` | `0x00000010` | `0xc` |
| first priority drop | `0x22000405` | `0x18000c06` | `0x00000000` | `0xc` |
| first deactivated | `0x02000405` | `0x18000c06` | `0x00000000` | `0xd` |
| second active | `0x02000405` | `0x28000c06` | `0x00010000` | `0xd` |
| second priority drop | `0x02000405` | `0x28000c06` | `0x00000000` | `0xd` |
| both deactivated | `0x02000405` | `0x08000c06` | `0x00000000` | `0xf` |
| EL2 clear | `0x00000000` | `0x00000000` | `0x00000000` | `0xf` |

At every snapshot, HCR and VMCR retain their exact values, MISR and both EISR
banks are zero, and LR2 and LR3 remain empty as reflected in ELRSR0. Returning
an HPPIR or IAR without the required CPUID bits, changing either source tag,
using duplicate VirtualIDs, acknowledging out of priority order, accepting
more or fewer than two deliveries, a changed LR/APR/RPR value, a maintenance
or physical IRQ, duplicate or reordered HVC, unexpected exception, reset,
hang, or early exit is forbidden.

The controlling rules are in sections 5.2.6, 5.3.8, 5.5.4, 5.5.7, and 5.5.14
of the [Arm Generic Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/).

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict H4f oracle. Preserve the frozen source revision, exact image and QEMU
hashes, toolchain, host, command line, linked-image audit, and lossless final
trace under `results/` before marking H4f complete. Any pre-fix forbidden
outcome must be preserved separately rather than overwritten by a later pass.

## Recorded QEMU result

The frozen implementation at `720149dbd00dc4eecb390125fd54afe5ce4fcc0c`
first exposed a missing CPUID field in qemu-pi4's `GICV_HPPIR` result, then
passed 100 consecutive fresh processes with focused fork fix
`8460833e53a91458fd3ba63ff19fb6c4932e8bb4` on 2026-08-22. The exact pre-fix
failure, passing image and emulator, environment, linked-image audit, and
lossless traces are recorded in
`results/2026-08-22-h4f-qemu-pi4/manifest.md`.

This remains a fork defect candidate, not an upstream-ready finding. It still
requires independent reproduction on unmodified current upstream master,
prior-report research, and manual user validation. No external report or
patch was sent.
