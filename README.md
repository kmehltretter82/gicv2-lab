# gicv2-lab

Deterministic AArch64 EL2 lab for differential GICv2 virtualization testing on
Raspberry Pi 4/400, QEMU, and Linux KVM.

gicv2-lab is a small bare-metal monitor and a collection of tiny EL1 test
payloads. It is not a production hypervisor. Its purpose is to make a virtual
interrupt transition observable, replayable, and comparable across:

1. the qemu-pi4 fork and unmodified QEMU under TCG;
2. a Pi 400's physical GIC-400 virtualization interface; and
3. Linux KVM's KVM_DEV_TYPE_ARM_VGIC_V2 interface.

A central goal is to find correctness defects in Linux arm64 KVM's VGICv2
implementation through small tests of documented behavior. The current
coverage and proposed extensions are recorded in [docs/KVM_COVERAGE.md](docs/KVM_COVERAGE.md).

## Status

H4i is complete under qemu-pi4. The QEMU-only lab now has:

- the H1 EL2 monitor and H2 one-vCPU EL1 guest with a validated, resumable
  stage-2 translation fault;
- a stage-2 mapping for exactly the 8 KiB GICV virtual CPU-interface block;
- dynamic List Register discovery and deterministic GICD/GICC/GICH setup;
- staged injection of software virtual INTIDs 42 and 43 through LR0 and LR1;
- EL1 acknowledgement and EOI through GICV, including one controlled nested
  current-EL IRQ;
- EL2 capture, validation, clearing, and physical EOI of maintenance PPI 25;
- exact empty, pending, active, post-EOI, and cleared LR snapshots, including
  their corresponding GICH_APR transitions;
- an isolated underflow-maintenance transition with two valid software LRs,
  a masked pending reserve, and no EOI/EISR cause;
- BPR 2 priority preemption from INTID 42 at priority `0x80` to INTID 43 at
  priority `0x20`, with exact both-active and reverse-completion checkpoints;
- split EOI mode for one software virtual interrupt, with guest-visible RPR
  and exact LR/APR state proving that EOIR drops priority while DIR performs
  the later deactivation; and
- one controlled Active-to-Pending+Active re-pend, with HPPIR eligibility,
  Pending exposure at the first DIR, exactly one second delivery, and final
  deactivation all checked against exact LR/APR state; and
- two software virtual SGIs with distinct source CPUID fields carried through
  raw HPPIR, IAR, split EOI, and deactivation values; and
- full four-LR exhaustion, one fifth interrupt held in an EL2 software queue,
  and a single validated empty-slot refill followed by five ordered
  completions; and
- an exact `HCR_EL2.TWI` trap and same-PC re-execution of guest WFI, followed
  by one `CNTHP_EL2` PPI and one timer-injected virtual wake interrupt; and
- a paused active-plus-pending virtual-interface context saved across HCR,
  VMCR, APR, and all four LRs, replaced by a distinct quiescent context, and
  restored with HCR enabled last before nested delivery and split completion.

The strict H4i exit gate passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22. Its exact revision, hashes, environment, linked-image audit, and
final raw trace are preserved in `results/2026-08-22-h4i-qemu-pi4/`. H4i
matched its frozen contract and produced no QEMU bug candidate. H4f's earlier
fork candidate and its separate pre-fix evidence remain recorded under the
H4f result directory.

All planned QEMU-only H4 milestones are complete.

The first physical boot has now been performed. On 2026-09-03 the unmodified
H4i image booted bare metal on a Pi 400 Rev 1.0 (BCM2711, GIC-400) through the
firmware's one-shot `tryboot`, was handed off at EL2, and reached
`[gicv2-lab] H4i PASS` on the physical GIC-400. Its serial trace was imported
into H5 and compared with a qemu-pi4 capture of the identical ELF: the
comparison reported `matches: true` across 25 ordered markers and 221
architectural field values, verified non-vacuous by a negative control. Six
raw lines differ, all identity or capability registers the scenario excludes
by design; two are unqualified fork-fidelity observations. Evidence is in
`results/2026-09-03-h6-pi400/` and the procedure in `docs/H6_PI400_BOOT.md`.

A historical follow-up passed 36 consecutive boots; see
`results/2026-09-03-h6-gate-pi400/`. On 2026-09-05 a fresh campaign completed
the full H6 exit gate: **100/100 consecutive boots passed**, each matching the
frozen qemu-pi4 baseline across 25 markers and 221 architectural fields.
Every raw capture and its complete source/image provenance is archived in
[results/2026-09-05-h6-pi400-100/manifest.md](results/2026-09-05-h6-pi400-100/manifest.md).
The archive passed verification after extraction to a fresh directory. The
original boot files were restored and verified, and the temporary image removed.

Unattended repetition is possible because the image arms the BCM2711 watchdog,
which warm-resets the halted board back to its vendor kernel.

Further hardware execution stays opt-in and requires an explicit request.
`tools/h6_gate.py` now prepares and audits a complete source/image/QEMU bundle
locally. The updated repetition driver retains every raw capture and locks
the board and serial port across result directories. That updated driver was
used for the completed 100-boot gate. See ROADMAP.md and docs/H6_PI400_BOOT.md.

The fork has also been compared with unmodified upstream QEMU for the first
time. On the H4i contract they agree exactly (`matches: true`). On the H4f
virtual-SGI contract they do not: upstream omits the SGI source CPUID from
`GICV_HPPIR`, which reproduces a previously fork-only defect candidate on
unmodified upstream; the responsible code was also found in the inspected
upstream master revision. See
`results/2026-09-03-h4f-upstream-qualification/` and
docs/H4F_MULTI_SOURCE_SGI.md. Nothing has been reported anywhere.

H5's QEMU-only host-tool baseline is complete: it captures raw serial output,
pins the ELF and scenario hashes, normalizes selected architectural state,
compares fresh runs, and reduces a mismatch to its first trace prefix. The
frozen 20-process gate and replay evidence are in
`results/2026-08-22-h5-qemu-pi4/`. It does not boot hardware. See
docs/H5_DIFFERENTIAL_RUNNER.md.

H7's minimal Linux arm64 KVM runner is implemented with a separate EL1 SPI
lifecycle guest that also runs under QEMU TCG. Build and capture bundles pin
the source, toolchain, image, and traces. On 2026-09-05 its existing GICv2
unit-test prerequisite passed all 17 checks on the Pi 400, and twenty fresh
H7 KVM captures matched QEMU across six markers and sixteen fields. Evidence
is in `results/2026-09-05-h7-pi400-kvm/`. See docs/H7_KVM.md.

H8 adds separate split-EOI, priority-mask/preemption-eligibility, and software
redelivery contracts. Each passed twenty QEMU captures. Under KVM on the
Pi 400 (2026-10-03, two kernels) all three fail for level-sensitive SPIs and
pass in an edge-triggered diagnostic build; the board's physical GIC-400
agrees with the contracts. See
[docs/H8_KVM_CONTRACTS.md](docs/H8_KVM_CONTRACTS.md),
[results/2026-09-05-h8-local/manifest.md](results/2026-09-05-h8-local/manifest.md) and
[results/2026-10-03-h8-pi400-kvm/manifest.md](results/2026-10-03-h8-pi400-kvm/manifest.md).

## Build

The project uses LLVM's freestanding AArch64 tools. On this Mac:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin

On a system where LLVM tools are on PATH:

    make

This produces:

- build/gicv2-lab.elf — direct-QEMU image;
- build/gicv2-lab.bin — raw image retained for later firmware boot work;
- build/gicv2-lab.map and build/gicv2-lab.dis — audit artifacts.

## QEMU smoke test

The default test uses the local sibling qemu-rpi4 checkout:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke

Override QEMU to test another build:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin \
      QEMU=/path/to/qemu-system-aarch64 smoke

Run the H4i exit-gate loop (100 consecutive boots by default) with:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin smoke-repeat

Set `SMOKE_RUNS=N` to select a different count.

The test loads the ELF directly at 0x80000 and uses
-cpu cortex-a72,has_el3=off. That prevents QEMU's optional synthetic EL3 from
obscuring the non-secure EL2 environment the Pi 400 exposes to this lab. It
checks the H1 and H2 descriptors and exceptions, the virtual-interrupt path,
every H4i HCR/VMCR/LR/APR/MISR/EISR/ELRSR snapshot, the exact saved and
restored writable context, the nested guest-visible IAR/RPR/HPPIR sequence,
and the final PASS marker. It never writes a physical boot medium.

The H2 architectural contract and fixed memory map are documented in
docs/H2_STAGE2.md. The virtual-interrupt contract is in docs/H3_GICV2.md, the
LR lifecycle is in docs/H4A_LR_LIFECYCLE.md, underflow maintenance is in
docs/H4B_UNDERFLOW.md, priority preemption is in
docs/H4C_PRIORITY_PREEMPTION.md, split EOI is in docs/H4D_SPLIT_EOI.md, and
Active+Pending redelivery is in docs/H4E_ACTIVE_PENDING.md. Multi-source SGIs
are in docs/H4F_MULTI_SOURCE_SGI.md, LR exhaustion/refill is in
docs/H4G_LR_REFILL.md, WFI wake is in docs/H4H_WFI_WAKE.md, paused
virtual-interface context save/restore is in
docs/H4I_CONTEXT_SAVE_RESTORE.md, and the guest call interface is in
docs/HVC_ABI.md. Physical Pi 400 boot, its `tryboot` procedure, and the serial
requirements are in docs/H6_PI400_BOOT.md.

## H5 differential runner

The host-tool unit tests require only Python 3's standard library:

    make h5-test

Capture one fresh, preserved QEMU result into a new directory:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin h5-qemu H5_OUT=build/h5/qemu-a

Run a fresh-process semantic repeat gate with `H5_RUNS` captures:

    make LLVM_BIN=/opt/homebrew/opt/llvm/bin h5-repeat \
      H5_RUNS=20 H5_REPEAT_OUT=build/h5/repeat-20

The generated directory contains lossless serial output, QEMU stderr, the
exact invocation and hashes, normalized trace, and comparisons. The runner
does not perform a Pi 400 or KVM action; those traces can only be imported
after the separately approved H6/H7 work. See docs/H5_DIFFERENTIAL_RUNNER.md
for replay, cross-backend comparison, allowed-outcome rules, and reduction.

## H7 local build and QEMU test

    make h5-test h6-test h7-test
    make LLVM_BIN=/opt/homebrew/opt/llvm/bin h7-build H7_BUILD=build/h7-frozen
    make QEMU=/path/to/qemu-system-aarch64 h7-qemu \
      H7_BUILD=build/h7-frozen H7_OUT=build/h7-qemu-20 H7_RUNS=20

Use a fresh directory for every build and result. These commands build locally
and run QEMU TCG. The separate KVM workflow and its completed first gate are
documented in docs/H7_KVM.md.

## Repository boundaries

The lab is deliberately separate from qemu-pi4: it must be able to test the
fork, unmodified QEMU, KVM, and hardware without becoming part of any one
system under test. It does not use a Git submodule; result manifests will pin
the tested revisions instead.

Source files are GPL-2.0-or-later. See LICENSE and docs/PROVENANCE.md.
