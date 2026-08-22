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

Add one scenario at a time: List Register state transitions, underflow and EOI
maintenance events, priority/preemption, EOImode, level reassertion,
multi-source SGIs, List Register overflow, WFI wakeup, and paused-vCPU
save/restore.

## H5 — differential runner

Run the same payload and scenario input on QEMU, real hardware, and KVM.
Compare only architecturally meaningful fields. Preserve raw traces and reduce
any one-sided forbidden outcome to its smallest sequence.

## H6 — first real Pi 400 boot (later)

The H1–H3 QEMU prerequisite is satisfied. Physical execution remains opt-in
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
