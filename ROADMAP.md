# Roadmap

The lab is useful when it produces a small, reproducible trace whose allowed
and forbidden outcomes are clear. Booting Linux is not the first objective.

A central goal is to find correctness defects in Linux arm64 KVM's VGICv2
implementation. H6 qualifies the physical testbed and H7 establishes a KVM
execution baseline. Further work should expand documented-behavior coverage
through Linux KVM and provide evidence that distinguishes an implementation
defect from a harness error or an allowed implementation difference.

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

### H4i — paused virtual-interface save and restore (completed)

- Pause at EL2 with one virtual interrupt active and another pending, then
  save HCR, VMCR, APR, and every implemented LR.
- Install and validate a distinct quiescent virtual-interface context before
  restoring the saved context exactly, with HCR enabled last.
- Resume and require both guest completions in order. This tests architectural
  vCPU-interface context switching, not QEMU migration or QMP state transfer.

Exit gate: the frozen strict H4i oracle passed 100 consecutive fresh qemu-pi4
processes without a save/restore mismatch, incorrect nested delivery,
maintenance event, physical IRQ, unexpected exception, reset, hang, or early
exit on 2026-08-22. The exact revision, linked-image audit, and raw trace are
preserved in `results/2026-08-22-h4i-qemu-pi4/`.

## H5 — differential runner (QEMU host-tool baseline completed)

Run the same payload and scenario input on QEMU, real hardware, and KVM.
Compare only architecturally meaningful fields. Preserve raw traces and reduce
any one-sided forbidden outcome to its smallest sequence.

The first H5 slice is intentionally QEMU-only and introduces:

- a versioned H4i scenario contract and canonical serial-trace format;
- lossless raw serial/stderr capture plus scenario, ELF, QEMU, and command
  provenance;
- a hard same-ELF-hash gate before a differential comparison;
- explicit equality and architecturally justified allowed-outcome comparison
  rules, rather than treating a Pi 400 implementation choice as the oracle;
- manifest-based fresh QEMU replay and N-process repeat comparison; and
- a first-difference trace-prefix reducer.

This baseline does not boot physical hardware or invoke KVM. H6/H7 output
will be imported passively into the same trace contract after those steps are
explicitly approved. The current reducer shortens diagnostic trace evidence;
data-driven operation-level delta reduction remains a later H5 extension.

Exit gate: the frozen implementation at
`2f0bea750192c96b2196f3a3e1e739f9cb2874d3` passed seven unit tests, a clean
build, one strict H4i UART oracle, twenty fresh QEMU TCG captures, and a
manifest-pinned replay on 2026-08-22. Every normalized repeat trace matched
run 1. The exact artifacts are retained in
`results/2026-08-22-h5-qemu-pi4/`.

## H6 — first real Pi 400 boot (100-boot gate completed)

The H1–H4i QEMU prerequisite is satisfied. Physical execution remains opt-in
and requires the user's explicit request. The first image:

1. uses a recoverable boot medium and serial capture;
2. makes no runtime storage, network, USB, PCIe, OTP, or firmware writes;
3. restricts MMIO to UART, timer, and documented GIC regions;
4. runs one core only;
5. enters the tiny EL1 guest, completes one HVC and one virtual interrupt;
6. prints the trace and halts.

Do not enable SMP, randomized sequences, repeated resets, guest-controlled
MMIO, or stage-2 fuzzing on that first hardware run.

### First boot (completed 2026-09-03)

The unmodified H4i image booted bare metal on a Pi 400 Rev 1.0 (BCM2711,
GIC-400) through the firmware's one-shot `tryboot`, was handed off at EL2
(`CurrentEL=2`), and reached `[gicv2-lab] H4i PASS` on the physical GIC-400.
All six constraints above held; the image booted is the full H4i payload,
which does more than constraint 5's minimum, chosen so that no source change
was introduced on the first physical boot and so the ELF hash matched the
QEMU baseline that H5's provenance gate requires.

The approved BCM2711 watchdog was not armed and remains unused: with a working
UART a hang is directly observable, so it would have added MMIO outside
constraint 3 and changed the ELF hash for no evidence gain.

Its serial trace was imported through `h5_runner.py import-trace` and compared
with a qemu-pi4 capture of the identical ELF. The comparison reported
`matches: true` over 25 ordered markers and 221 architectural field values,
with a negative control confirming it was not vacuous. Six raw lines differ,
all identity or capability registers the scenario excludes by design; two of
them — the physical `GICC_PMR` priority width and `VTCR_EL2` bit 31 at reset —
are fork-fidelity observations that have not been qualified for a report. The
exact revision, hashes, boot configuration, both raw traces, and the
comparison are preserved in `results/2026-09-03-h6-pi400/`. See
`docs/H6_PI400_BOOT.md`.

### Repetition (2026-09-03)

Adding the BCM2711 watchdog to the image made unattended repetition possible:
the monitor halts, the watchdog warm-resets, the one-shot tryboot selection is
already consumed, and the board returns to its vendor kernel by itself.
`scripts/h6-repeat.sh` drove 36 consecutive boots, each compared with a frozen
qemu-pi4 baseline of the identical ELF. All 36 passed, and all 36 trace bodies
share one SHA-256 at a constant 11481 bytes.

An attempt to extend the run past 36 was invalidated by two concurrent gate
instances competing for the one serial port, which truncated each other's
captures; `import-trace` rejected them and those runs were discarded. The
script now takes an atomic lock to prevent it. Evidence and the full account
are in `results/2026-09-03-h6-gate-pi400/`.

Exit gate: **passed on 2026-09-05**. The gate is 100 consecutive fresh hardware boots whose
traces each compare equal to a frozen qemu-pi4 baseline, with no timeout, FAIL
marker, or provenance rejection. A fresh campaign passed all 100 comparisons
with every raw capture, frozen image/source bundle, command and board identity
retained. The complete archive passed a second audit after relocation. Original
boot-file contents were restored and verified; the temporary image was removed.
See `results/2026-09-05-h6-pi400-100/manifest.md`.

### Host preparation and capture integrity

`tools/h6_gate.py` now prepares a source/image/QEMU bundle entirely locally,
verifies its hashes, and archives all evidence. The repetition driver locks
the board and serial device across output directories, retains every raw
capture, checks deployed image bytes, and refuses to overwrite, skip, or
resume after failed/incomplete attempts. The updated driver completed the
100-boot campaign above. See `docs/H6_PI400_BOOT.md`.

## H7 — Linux/KVM workload layer (first gate completed)

The minimal `/dev/kvm` runner and its separate one-vCPU EL1 SPI lifecycle
payload are implemented. The same H7 ELF can run under QEMU TCG and KVM's
VGICv2 device API; the H5 hash gate and comparison format apply unchanged.

Before KVM capture, the runner requires a preserved passing
`kvm-unit-tests` `gicv2-mmio-up` result under KVM on the same host/kernel.
On 2026-09-05 that unmodified upstream test passed all 17 checks on the
Pi 400's vendor `6.18.39+rpt-rpi-v8` kernel. Twenty subsequent fresh H7 KVM
captures passed and matched the QEMU TCG baseline of the identical ELF across
six ordered markers and sixteen architectural fields. Complete evidence,
including every raw trace and frozen source/image bundle, is retained in
`results/2026-09-05-h7-pi400-kvm/`. A native Linux UAPI check passed and a
synthetic IAR-value mutation was rejected by the comparator.

Exit gate: **passed**. The existing KVM test passes, the H7 guest passes under
KVM, and its preserved trace compares equal to QEMU's trace of the identical ELF.
See `docs/H7_KVM.md` for the contract and commands. Add guest PSCI, virtual
timer, console extensions, and a Linux device tree only when a focused Linux
workload needs them.

## H8 — Linux VGICv2 correctness coverage (first contracts implemented)

Maintain the evidence-based matrix in `docs/KVM_COVERAGE.md`. Prioritize
uncovered interrupt-state transitions and documented state-preservation
behavior, starting with focused single-vCPU scenarios. The existing H4
payloads directly manage EL2 state; their passing results do not establish
Linux KVM coverage.

Three single-vCPU additions are implemented: split priority drop/deactivation,
priority masking and preemption eligibility, and software-pending redelivery.
Each has a separate versioned contract, frozen image and twenty passing QEMU
captures. Synthetic forbidden-value, truncation and marker-order controls were
rejected. See `docs/H8_KVM_CONTRACTS.md` and
`results/2026-09-05-h8-local/manifest.md`.

The frozen bundle ran under KVM on the Pi 400 on 2026-10-03. All three
contracts failed, deterministically and identically on the vendor
`6.18.39+rpt-rpi-v8` kernel and on a 7.3-rc3-based kernel: for a
level-sensitive SPI pended through GICD_ISPENDR, KVM keeps the pending bit set
while the interrupt is active and drops a second pending write made while it
is active. Edge-triggered diagnostic builds pass and match QEMU. A probe of
the board's physical GIC-400 agrees with the contracts and with QEMU, as does
IHI 0048B.b Table 4-11 and Figure 4-10. This is a recorded observation with a
known mechanism in the vgic code, not yet a qualified defect report; the
GICv3 path, practical impact and a wider duplicate search remain open. See
`results/2026-10-03-h8-pi400-kvm/manifest.md`.

Add documented KVM state-save/restore checks after those guest-visible contracts
are established. A full Linux guest is not required for this phase.

Track coverage across recorded kernel revisions with identical test inputs.
An unexpected result requires review of the rule, preconditions, harness,
and implementation-defined behavior before it becomes a defect candidate.
Retain a minimal regression test and complete evidence for any confirmed
correctness defect; external reporting remains separately authorized.

Exit gate: the selected contracts have auditable KVM evidence and the
coverage matrix reflects both passing cases and unresolved observations.
Finding a previously unknown defect is a research outcome, not a result
that a fixed number of passing executions can guarantee.
