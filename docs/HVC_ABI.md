# H2/H3 guest call ABI

H2 and H3 use a deliberately private call ABI. It is not SMCCC and is not
intended for Linux guests.

The AArch64 guest executes `HVC #0` with the operation in `x0` and its single
argument in `x1`. A returning call receives status zero in `x0`; all other
general-purpose registers are restored. `PASS`, `FAIL`, and `EXIT` are trace
protocol operations rather than general hypervisor services.

| `x0` | Name | `x1` | Result |
| ---: | --- | --- | --- |
| `0x100` | REPORT | raw `CurrentEL`; H2 requires `4` (EL1) | mark the guest report seen and return |
| `0x101` | PASS | magic `0x600d` | require the report and expected fault, then return |
| `0x102` | FAIL | scenario-defined | print a failure and halt |
| `0x103` | EXIT | zero | require the complete H3 sequence, print H3 PASS, and halt |
| `0x104` | IRQ_READY | GICV `CTLR` in bits 31:0 and `PMR` in bits 63:32 | validate the virtual interface, inject virtual INTID 42, and return |
| `0x105` | IRQ_EOI | the value read from `GICV_IAR`; H3 requires 42 | mark the guest virtual-IRQ acknowledgement seen and return |

The H2 prefix is REPORT, the deliberate data abort, then PASS. H3 continues
with IRQ_READY, injects the virtual IRQ during that call, and ends with both an
IRQ_EOI report and a maintenance PPI acknowledgement before EXIT. The
maintenance interrupt is asynchronous and can reach EL2 immediately after
the guest's `GICV_EOIR` write, before the subsequent IRQ_EOI HVC. The monitor
therefore accepts either ordering of those two observations while requiring
both exactly once.

The monitor rejects duplicate calls, invalid arguments, unknown operations,
and an early exit. An HVC resumes at the architecturally supplied return
address; the monitor does not increment `ELR_EL2` for HVC calls.
