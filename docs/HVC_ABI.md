# H2-H4f guest call ABI

H2 through H4f use a deliberately private call ABI. It is not SMCCC and is
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
| `0x103` | EXIT | zero | require the complete H4f sequence, print H4f PASS, and halt |
| `0x104` | IRQ_READY | GICV `CTLR` in bits 31:0, `PMR` in bits 39:32, `BPR` in bits 42:40, all other bits zero | require `CTLR=0x201`, validate the virtual interface, inject both source-tagged SGIs, and return |
| `0x105` | SGI_PENDING | raw HPPIR | require SGI 5 with CPUID 1 (`0x0405`) before IRQ unmask |
| `0x106` | SGI_FIRST_ACTIVE | SGI interrupt-state tuple | validate the first raw IAR, source tag, Active LR0, APR, RPR, and pending second SGI |
| `0x107` | SGI_FIRST_EOI | SGI interrupt-state tuple | validate the first priority drop and unchanged second source tag |
| `0x108` | SGI_FIRST_DEACTIVATE | SGI interrupt-state tuple | validate invalid LR0 and still-pending LR1 |
| `0x109` | SGI_SECOND_ACTIVE | SGI interrupt-state tuple | validate the second raw IAR, source tag, Active LR1, APR, and RPR |
| `0x10a` | SGI_SECOND_EOI | SGI interrupt-state tuple | validate the second priority drop |
| `0x10b` | SGI_SECOND_DEACTIVATE | SGI interrupt-state tuple | validate both invalid LRs, clear them, and return |

For H4e, an interrupt-state tuple places the raw GICV IAR in bits 31:0,
GICV RPR in bits 39:32, and the low ten bits of GICV HPPIR in bits 49:40. All
other bits are zero. Every state report requires IAR 42. Both active calls
require RPR `0x80` and HPPIR 1023. Both EOI calls require RPR `0xff` and HPPIR
1023. The first deactivation call requires RPR `0xff` and HPPIR 42; the final
deactivation call requires RPR `0xff` and HPPIR 1023.

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

H4d keeps one LR and enables `GICV_CTLR.EOImode`. IRQ_ACTIVE precedes the
guest's EOIR write. IRQ_EOI then requires the priority to have dropped while
LR0 remains active. Only after that call returns does the guest write the same
IAR value to GICV_DIR and call IRQ_DEACTIVATE. EL2 requires LR0 invalid,
clears it, and accepts EXIT only after all three checkpoints occurred once in
that order. H4d also includes GICV_RPR in each interrupt-state argument so the
guest-visible running-priority transition is checked directly.

All H4d maintenance enables remain clear. A physical IRQ is forbidden.

H4e retains split EOI mode and adds one controlled re-pend. The first
IRQ_ACTIVE call validates Active LR0 and then changes only its State field to
Pending+Active. IRQ_EOI observes priority drop without changing that state.
After the first DIR, IRQ_DEACTIVATE requires LR0 Pending and deliberately does
not clear it. The first handler returns, allowing one second acknowledgement;
IRQ_REDELIVERED validates Active LR0 and APR bit 16. IRQ_SECOND_EOI observes
the final priority drop, and IRQ_SECOND_DEACTIVATE validates the final DIR and
clears LR0. EXIT is accepted only after those six calls occur once in that
order. HPPIR proves that the Active+Pending entry was not eligible before
deactivation and that the resulting Pending entry was eligible afterward.

All H4e maintenance enables remain clear. A physical IRQ is forbidden.

H4f replaces the H4e operations from `0x105` onward with the source-tagged
SGI calls in the current table. An SGI interrupt-state tuple places the raw
GICV IAR, including CPUID bits [12:10], in bits 31:0; RPR in bits 39:32; and
the low 13 raw HPPIR bits, including CPUID, in bits 52:40. All other bits are
zero. The first tuple requires IAR `0x0405`; while the second SGI is pending,
HPPIR must be `0x0c06`. The second tuple requires IAR `0x0c06`; after that
acknowledgement HPPIR must be 1023. EXIT is accepted only after the two
split-EOI completion sequences occur once in priority order.

All H4f maintenance enables remain clear. A physical IRQ is forbidden.

The monitor rejects duplicate calls, invalid arguments, unknown operations,
and an early exit. An HVC resumes at the architecturally supplied return
address; the monitor does not increment `ELR_EL2` for HVC calls.
