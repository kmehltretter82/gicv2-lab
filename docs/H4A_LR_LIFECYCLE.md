# H4a List Register lifecycle

## Architectural contract

H4a extends the H3 virtual-interrupt scenario with an EL2 checkpoint after
EL1 acknowledges virtual INTID 42 but before EL1 writes `GICV_EOIR`. This
makes the complete LR0 lifecycle and the corresponding active-priority state
directly observable.

The H3 preconditions remain in force. In particular, LR0 contains a software
virtual interrupt with priority `0x20` and EOI maintenance enabled, and
`GICH_VMCR=0xf8fc0001` configures both virtual binary points to 7. H4a uses one
vCPU, does not execute WFI, and does not exercise preemption or priority
competition.

The guest IRQ handler reads `GICV_IAR`, requires INTID 42, and executes
`HVC_IRQ_ACTIVE` while retaining that raw IAR value in `x1`. EL2 then reads
the virtual-control registers without modifying them and returns to the guest.
Only after that checkpoint does the guest write `GICV_EOIR`.

The five required snapshots are:

| Checkpoint | LR0 state | LR0 value | APR | MISR | EISR0 | ELRSR0 with 4 LRs |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| initialized | invalid/empty | `0x00000000` | `0x0` | `0x0` | `0x0` | `0xf` |
| injected | pending | `0x1208002a` | `0x0` | `0x0` | `0x0` | `0xe` |
| acknowledged | active | `0x2208002a` | `0x1` | `0x0` | `0x0` | `0xe` |
| guest EOI | invalid, EOI status retained | `0x0208002a` | `0x0` | `0x1` | `0x1` | `0xe` |
| EL2 clear | invalid/empty | `0x00000000` | `0x0` | `0x0` | `0x0` | `0xf` |

At every checkpoint, `GICH_HCR` must remain `0x1` and `GICH_VMCR` must remain
`0xf8fc0001`. `EISR1` and `ELRSR1` are zero for the four-LR qemu-pi4 model.
The implementation derives the meaningful `ELRSR` masks from the LR count,
so the internal validator is not limited to four LRs.

Reading `GICV_IAR` changes the matching LR from pending to active and records
its active preemption level in `GICH_APR`. With the configured virtual binary
point of 7, priority `0x20` belongs to preemption level zero, represented by
APR bit 0. Writing `GICV_EOIR` clears that active state and APR bit. Because
LR0 has EOI maintenance enabled, the invalid LR retains its EOI status and
raises the EOI maintenance condition until EL2 clears LR0.

The maintenance PPI may arrive before or after the guest's later
`HVC_IRQ_EOI` report. Both orderings remain permitted, but the active
checkpoint must precede the guest EOI and both observations must occur exactly
once before EXIT.

A missing or duplicate checkpoint, a wrong IAR, a changed control register,
any other LR/APR/MISR/EISR/ELRSR value, an unexpected exception, reset, hang,
or early exit is forbidden. At the recorded H4a source revision
`c965ac10b0f539ff54ff7903650a70c354f10e69`, the strict oracle was
`scripts/smoke-qemu.sh`. Later milestones reuse that filename for their current
scenario, so the recorded revision and result manifest pin the H4a oracle.

Register encodings and state transitions are defined by the [Arm Generic
Interrupt Controller Architecture Specification, version
2](https://developer.arm.com/documentation/ihi0048/latest/), particularly the
virtual CPU interface and hypervisor-control-interface register descriptions.

## Exit gate

The exit gate is 100 consecutive fresh qemu-pi4 processes accepted by the
strict oracle, with no forbidden outcome. The tested source revision, image
and QEMU hashes, toolchain, host, exact command, and lossless final serial
trace must be preserved under `results/` before H4a is marked complete.

## Recorded result

The strict oracle passed 100 consecutive fresh qemu-pi4 processes on
2026-08-22 with no forbidden outcome. The exact tested lab revision, artifact
and QEMU binary hashes, toolchain, command line, host environment, and final
raw serial trace are recorded in
`../results/2026-08-22-h4a-qemu-pi4/manifest.md`.
