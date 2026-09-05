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

## Upstream qualification (2026-09-03)

The defect is no longer only a fork candidate: it reproduces on **unmodified
upstream QEMU**. The frozen H4f ELF
`ae78b410d9df46fabb567b6001794965f1b96d3f81390f4d5b5328c83e2c0391` was run on
an unmodified upstream 11.0.2 release binary under `-machine raspi4b`, and
diverged from the fork at the first source-tag checkpoint:

    fork     GuestHPPIR_both_pending_observed=0x405
    upstream GuestHPPIR_both_pending_observed=0x005

The difference is exactly `0x400`, CPUID 1 in `GICV_HPPIR[12:10]`. The matching
List Register is `GICH_LR0=0x22000405`, VirtualID 5 with CPUID 1, so `0x405` is
the architecturally required value. Upstream then fails this scenario's own
oracle with `H4f FAIL: invalid initial SGI HPPIR report`.

Upstream's `gic_cpu_read()` serves `GICV_HPPIR` from
`gic_get_current_pending_irq()`, which returns the bare INTID, while
`gic_acknowledge_irq()` behind `GICV_IAR` does apply the List Register's CPUID.
Upstream therefore reports a different source tag through IAR than through
HPPIR for the same pending virtual SGI.

The code is still present on upstream master `a925240509` (2026-09-02), and no
upstream commit addresses it; the only commits touching
`gic_get_current_pending_irq` are `c5619bf9e8` and `7c0fa108d9`, the latter
concerning the physical interface's grouping.

Full evidence, both raw traces, and the exact backends are preserved in
`results/2026-09-03-h4f-upstream-qualification/`.

### What is still outstanding

- **Prior-report research is incomplete.** Upstream git history shows no fix. A
  local archive search of qemu-devel covering 2026-06-24 to 2026-09-03 (20000
  messages) found no mention of `HPPIR` at all. That window is far shorter than
  the age of the code, so an older report cannot be ruled out from here.
  lore's web search is behind Anubis and unusable from scripts; a complete
  check needs a browser query such as `HPPIR` or `GICV_HPPIR` on
  `lore.kernel.org/qemu-devel/`.
- **User review.** Nothing has been sent anywhere, and nothing should be
  without it.
- **The fork's fix cannot simply be posted.** `8460833e53` carries an
  `Assisted-by: OpenAI Codex` trailer. AGENTS.md forbids turning AI-assisted
  work from this repository into a QEMU upstream patch, and QEMU's
  code-provenance policy has to be satisfied independently. That constrains the
  *patch*; whether to file a defect *report* with a reproducer is a separate
  decision and remains the user's.
