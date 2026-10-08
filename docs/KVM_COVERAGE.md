# Linux arm64 KVM VGICv2 coverage

The research objective is to find correctness defects in Linux KVM's VGICv2
implementation using small tests of architectural and documented API behavior.
This matrix records demonstrated coverage and identifies useful extensions.
It does not count QEMU-only or bare-metal coverage as Linux KVM coverage.

## Verified baseline

The 2026-09-05 KVM evidence comes from one Pi 400 running vendor kernel
`6.18.39+rpt-rpi-v8`. The upstream `gicv2-mmio-up` prerequisite passed 17
checks; twenty H7 guest captures passed and matched QEMU's trace of the same
ELF. See [the hardware manifest](../results/2026-09-05-h7-pi400-kvm/manifest.md).
These results contain no Linux KVM defect candidate.

| Behavior | Existing lab evidence | Linux KVM coverage |
| --- | --- | --- |
| Selected distributor identity, priority and target register semantics | Upstream `gicv2-mmio-up`, one vCPU, 17 checks | Verified for that test and host/kernel |
| One software-pended SPI: pending, acknowledge, active, combined EOI, quiescent | H7, six markers and sixteen fields | Verified; twenty fresh captures |
| Split priority drop and deactivation | H4d directly controls GICH/GICV; H8a EL1 contract passes QEMU 20/20 | H8a ran 2026-10-03: fails for a level-sensitive SPI (pending bit stays set after acknowledge); edge-triggered diagnostic passes |
| Priority masking and preemption eligibility | H8b polls IAR with DAIF masked; QEMU 20/20 | H8b ran 2026-10-03: fails at the same pending-bit readback for level-sensitive SPIs; edge-triggered diagnostic passes; nested IRQ-handler entry remains separate |
| Active-plus-pending redelivery | H4e uses EL2-controlled state; H8c software-pending contract passes QEMU 20/20 | H8c ran 2026-10-03: for a level-sensitive SPI the second software pend made while active is not delivered; edge-triggered diagnostic passes |
| Edge- and level-triggered line behavior | H7 uses only a software pending event, with the input line deasserted | External-line semantics remain uncovered |
| SGI source identity | H4f uses synthetic source fields in virtual List Registers | No cross-vCPU KVM scenario yet |
| Delivery beyond immediate hardware LR capacity | H4g uses a lab-owned EL2 software queue | Does not exercise Linux KVM's queueing |
| Interrupt wakeup from a guest wait | H4h uses an EL2 timer and controlled WFI | No equivalent KVM wakeup scenario yet |
| Paused state preservation | H4i swaps lab-owned virtual-interface contexts | Does not exercise the KVM userspace save/restore interface |

H7 runs at EL1, so it observes the guest GIC interface while Linux owns the
EL2 implementation. Port the guest-visible contract of a useful H4 scenario;
do not assume that the H4 monitor itself can execute as a normal KVM guest or
that Linux must expose the lab's internal LR arrangement.

## Proposed order

The preserved upstream checkout also contains `gicv2-ipi`, `gicv2-active`,
`gicv2-mmio` and `gicv2-mmio-3p`. Those selections were not run in this
campaign. Review and reuse their relevant coverage before writing an
overlapping lab test. Their configured vCPU counts can exceed one, so each
selection needs a focused reason to expand the lab's current scope. The
exact definitions are in the archived `arm/unittests.cfg` at upstream commit
`f20b0b8d3373ab489ef3ed9f2ebdf6839bcddd9c`.

1. Run the implemented [H8 single-vCPU contracts](H8_KVM_CONTRACTS.md) under
   KVM using their frozen QEMU-qualified bundles. Their local evidence is in
   [the H8 manifest](../results/2026-09-05-h8-local/manifest.md). Done on
   2026-10-03 on two kernels; the level-sensitive observation, the physical
   GIC-400 reference and its limits are in
   [the KVM manifest](../results/2026-10-03-h8-pi400-kvm/manifest.md). It has
   not been qualified as a defect report.
2. Extend the fixed workload only as needed to test normal edge/level line
   semantics and documented paused state preservation.
3. Record results across kernel revisions with the image and scenario held
   fixed. Preserve kernel source revision, configuration and image identity,
   in addition to the existing guest and trace provenance.
4. Introduce a second vCPU only for a focused routing or SGI contract that
   needs it. Wait/wakeup and timer workloads are similarly separate additions.

The Linux userspace VGIC API includes representations that differ from the
guest's hardware register view. State-preservation tests must follow those
documented semantics, supported operations and stopped-vCPU preconditions.
See the [Linux VGICv2 device API](https://docs.kernel.org/virt/kvm/devices/arm-vgic.html)
and [KVM API](https://docs.kernel.org/virt/kvm/api.html).

## Interpreting results

Define allowed outcomes from the architecture or the documented Linux ABI
before running a test. QEMU and physical hardware provide useful independent
observations, but neither is a universal oracle for implementation-defined
behavior. A difference alone is not a Linux defect.

For an unexpected result, preserve the original evidence and review the
preconditions, test harness, synchronization and allowed implementation
choices. A confirmed candidate needs a reproducible violated contract and a
small regression test. Keep the coverage matrix honest about both unresolved
observations and the limits of passing cases. External reports require the
user's explicit request, as specified in `AGENTS.md`.
