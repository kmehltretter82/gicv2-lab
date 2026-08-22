# Roadmap

The lab is useful when it produces a small, reproducible trace whose allowed
and forbidden outcomes are clear. Booting Linux is not the first objective.

## H0 — scope and safety

- Maintain a stable trace format and result manifest.
- Keep a fixed physical-memory map for the monitor, guest, page tables,
  stacks, and trace buffer.
- Make physical Pi runs opt-in and recoverable.

## H1 — EL2 monitor under QEMU (completed)

- Reset at EL2, set a stack, clear BSS, and install VBAR_EL2.
- Initialize PL011 and print CPU, timer, and GIC capability registers.
- Take a deliberate EL2 synchronous exception and report its syndrome.
- Emit an H1 PASS checkpoint after returning from the deliberate exception.

Exit gate: 100 normalized cold-boot traces under qemu-pi4 complete without an
unexplained reset or hang. This is satisfied by the stricter H2 repetition
gate, which also runs the complete H1 checkpoint on every boot.

## H2 — one tiny EL1 guest (completed)

- Construct minimal stage-2 tables.
- Enter a one-vCPU AArch64 EL1 payload with ERET.
- Implement HVC report, pass, fail, and exit calls.
- Demonstrate an expected stage-2 fault.

Exit gate: the strict H2 smoke oracle passes 100 consecutive qemu-pi4 boots
without an unexpected exception, reset, or hang. Passed with 100 fresh QEMU
processes on 2026-08-22.

## H3 — first virtual interrupt (completed)

- Read the List Register count from GICH_VTR; do not assume four.
- Initialize GICH/GICV state and inject one virtual interrupt.
- Have the EL1 payload acknowledge and EOI it.
- Record and acknowledge the GIC maintenance PPI.

Exit gate: the strict H3 smoke oracle passed 100 consecutive qemu-pi4
processes without an unexpected exception, register state, reset, or hang on
2026-08-22. The tested source revision and raw trace are preserved in
`results/2026-08-22-h3-qemu-pi4/`.

## H4 — deterministic GICv2 state machine

### H4a — List Register lifecycle (completed)

- Snapshot LR0 and the hypervisor control state while LR0 is empty, pending,
  active, invalid after EOI, and empty again after EL2 clears it.
- Validate the matching GICH_APR transition at the active checkpoint.
- Keep the existing EOI maintenance event isolated from WFI and preemption.

Exit gate: the strict H4a smoke oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected exception, register state, reset, or hang on
2026-08-22. The tested source revision and raw trace are preserved in
`results/2026-08-22-h4a-qemu-pi4/`.

### H4b — underflow maintenance (completed)

- Start with exactly two valid pending software LRs and EOI maintenance
  disabled so the underflow cause is isolated.
- Enable underflow maintenance and have the guest acknowledge and EOI the
  higher-priority interrupt, reducing the valid-LR count from two to one.
- Require `GICH_MISR.U`, no EISR bit, the expected remaining pending LR, and a
  single maintenance PPI before restoring an empty, quiescent interface.

Exit gate: the strict H4b smoke oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected exception, register state, reset, or hang on
2026-08-22. The tested source revision and raw trace are preserved in
`results/2026-08-22-h4b-qemu-pi4/`.

### H4c — virtual priority and preemption (completed)

- Make one lower-priority virtual interrupt active before injecting a second
  interrupt at a genuinely higher preemption level.
- Permit one controlled nested EL1 IRQ and validate the LR state, APR bits,
  acknowledgement order, and reverse completion order at each checkpoint.
- Keep maintenance causes disabled so the priority/preemption transition is
  isolated.

Exit gate: the frozen strict H4c oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected exception, register state, reset, hang,
maintenance event, or physical IRQ on 2026-08-22. The tested source revision,
linked vector audit, and raw trace are preserved in
`results/2026-08-22-h4c-qemu-pi4/`.

### H4d — split priority drop and deactivation (completed)

- Enable the virtual CPU interface's split EOI mode for one focused software
  virtual interrupt.
- Observe and distinguish the priority drop performed by GICV_EOIR from the
  later deactivation performed by GICV_DIR.
- Validate the exact LR, APR, MISR, EISR, ELRSR, and guest-visible RPR state
  at the active, priority-drop, deactivated, and cleared checkpoints.

Exit gate: the frozen strict H4d oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected exception, register state, reset, hang,
maintenance event, or physical IRQ on 2026-08-22. The tested source revision,
stage-2 and linked-image audit, and raw trace are preserved in
`results/2026-08-22-h4d-qemu-pi4/`.

### H4e — Active+Pending redelivery (completed)

- Re-pend one software virtual interrupt while its first delivery remains
  active, producing the List Register Pending+Active state.
- In split EOI mode, prove that EOIR only drops priority and that the first
  DIR converts Pending+Active back to Pending.
- Require HPPIR eligibility only after deactivation, exactly one second
  acknowledgement, and a final Active-to-Invalid transition.

Exit gate: the frozen strict H4e oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected exception, register state, reset, hang,
maintenance event, physical IRQ, or extra/missing delivery on 2026-08-22. The
tested source revision, stage-2 and linked-image audit, and raw trace are
preserved in `results/2026-08-22-h4e-qemu-pi4/`.

### H4f — virtual SGI source tags (completed)

- Install two software-originated virtual SGIs with unique VirtualIDs and
  distinct CPUID source fields.
- Require each raw source tag in HPPIR and IAR and preserve it through split
  priority drop and deactivation.
- Keep maintenance and physical interrupts disabled so the source-field
  contract is isolated.

Exit gate: the frozen H4f oracle passed 100 consecutive fresh qemu-pi4
processes with focused fork fix `8460833e53` on 2026-08-22. The same frozen
image's pre-fix forbidden outcome, exact revisions, linked-image audit, and
raw traces are preserved in `results/2026-08-22-h4f-qemu-pi4/`. The result is
a fork defect candidate and has not been qualified for an upstream report.

### H4g — List Register exhaustion and refill (completed)

- Fill every implemented LR with a unique pending virtual interrupt and hold
  one lower-priority interrupt in an EL2 software queue.
- After the first guest completion makes one LR empty, refill exactly that LR
  and require all five interrupts in deterministic priority order.
- Validate every LR, APR, ELRSR, IAR, RPR, and HPPIR transition without using
  a maintenance or physical interrupt as an implicit refill trigger.

Exit gate: the frozen strict H4g oracle passed 100 consecutive fresh qemu-pi4
processes without an unexpected state, exception, reset, hang, maintenance
event, physical IRQ, or extra/missing delivery on 2026-08-22. The exact
revision, linked-image audit, and raw trace are preserved in
`results/2026-08-22-h4g-qemu-pi4/`.

### H4h — WFI wakeup (completed)

- Trap the guest's first WFI with HCR.TWI to prove the exact wait instruction
  and checkpoint, then re-execute it untrapped.
- Arm one EL2 physical timer event and use it to inject exactly one virtual
  interrupt that wakes the waiting guest.
- Forbid every unrelated physical interrupt, maintenance cause, duplicate
  wake, or completion without the validated WFI trap.

Exit gate: the frozen strict H4h oracle passed 100 consecutive fresh qemu-pi4
processes without a missing or skipped WFI, wrong timer return PC, unrelated
physical IRQ, maintenance event, extra/missing wake, unexpected exception,
reset, or hang on 2026-08-22. The exact revision, linked-image audit, and raw
trace are preserved in `results/2026-08-22-h4h-qemu-pi4/`.

### H4i — paused virtual-interface save and restore (exit gate pending)

- Pause at EL2 with one virtual interrupt active and another pending, then
  save HCR, VMCR, APR, and every implemented LR.
- Install and validate a distinct quiescent virtual-interface context before
  restoring the saved context exactly, with HCR enabled last.
- Resume and require both guest completions in order. This tests architectural
  vCPU-interface context switching, not QEMU migration or QMP state transfer.

Exit gate: 100 consecutive fresh qemu-pi4 processes accepted by a frozen
strict oracle.

The implementation and strict oracle pass a single-process pre-gate run. The
100-process repetition gate and evidence manifest remain to be completed.

## H5 — differential runner

Run the same payload and scenario input on QEMU, real hardware, and KVM.
Compare only architecturally meaningful fields. Preserve raw traces and reduce
any one-sided forbidden outcome to its smallest sequence.

## H6 — first real Pi 400 boot (later)

The H1–H4h QEMU prerequisite is satisfied. Physical execution remains opt-in
and requires the user's explicit request. The first image:

1. uses a recoverable boot medium and serial capture;
2. makes no runtime storage, network, USB, PCIe, OTP, or firmware writes;
3. restricts MMIO to UART, timer, and documented GIC regions;
4. runs one core only;
5. enters the tiny EL1 guest, completes one HVC and one virtual interrupt;
6. prints the trace and halts.

Do not enable SMP, randomized sequences, repeated resets, guest-controlled
MMIO, or stage-2 fuzzing on that first hardware run.

## H7 — Linux/KVM workload layer

First run existing GICv2 KVM unit tests. Then add a small /dev/kvm runner and,
only when it adds evidence, the minimum guest PSCI, virtual timer, console, and
device-tree support needed by a Linux guest.
