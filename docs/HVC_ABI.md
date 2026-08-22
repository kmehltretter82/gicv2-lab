# H2-H4c guest call ABI

H2 through H4c use a deliberately private call ABI. It is not SMCCC and is
not intended for Linux guests.

The AArch64 guest executes `HVC #0` with the operation in `x0` and its single
argument in `x1`. A returning call receives status zero in `x0`; all other
general-purpose registers are restored. `PASS`, `FAIL`, and `EXIT` are trace
protocol operations rather than general hypervisor services.

| `x0` | Name | `x1` | Result |
| ---: | --- | --- | --- |
| `0x100` | REPORT | raw `CurrentEL`; H2 requires `4` (EL1) | mark the guest report seen and return |
| `0x101` | PASS | magic `0x600d` | require the report and expected fault, then return |
| `0x102` | FAIL | scenario-defined | print a failure and halt |
| `0x103` | EXIT | zero | require the complete H4c sequence, print H4c PASS, and halt |
| `0x104` | IRQ_READY | GICV `CTLR` in bits 31:0, `PMR` in bits 39:32, `BPR` in bits 42:40, all other bits zero | validate the virtual interface, inject low-priority INTID 42 into LR0, and return |
| `0x105` | IRQ_EOI | the value read from `GICV_IAR`; H4c requires 42 | validate low EOI after nested completion, clear both LRs, and return |
| `0x106` | IRQ_ACTIVE | the value read from `GICV_IAR`; H4c requires 42 | validate active low-priority LR0, inject high-priority INTID 43 into LR1, and return |
| `0x107` | IRQ_NESTED_ACTIVE | the value read from `GICV_IAR`; H4c requires 43 | validate that both LRs and both APR bits are active, then return |
| `0x108` | IRQ_NESTED_EOI | the value read from `GICV_IAR`; H4c requires 43 | validate high EOI while low LR0 remains active, then return |

The H2 prefix is REPORT, the deliberate data abort, then PASS. H3 continues
with IRQ_READY and injects the virtual IRQ during that call. H4a adds
IRQ_ACTIVE after the guest reads `GICV_IAR` but before it writes `GICV_EOIR`.
It therefore observes LR0 and APR while the interrupt is active, rather than
merely reporting that the guest handler ran. H4b used a second masked LR to
exercise underflow maintenance.

H4c changes IRQ_READY to inject only the low-priority LR0. IRQ_ACTIVE is
called from the low handler with IRQs still masked; EL2 validates LR0 and
then injects the higher-priority LR1. The low handler explicitly unmasks
IRQs, allowing exactly one current-EL nested IRQ. The nested handler calls
IRQ_NESTED_ACTIVE, EOIs LR1, calls IRQ_NESTED_EOI, and returns. Only then does
the low handler mask IRQs, EOI LR0, and call IRQ_EOI. Every call and the final
EXIT is accepted once and only in that order.

All H4c maintenance enables remain clear. A physical IRQ is therefore a
forbidden outcome rather than part of the call sequence.

The monitor rejects duplicate calls, invalid arguments, unknown operations,
and an early exit. An HVC resumes at the architecturally supplied return
address; the monitor does not increment `ELR_EL2` for HVC calls.
