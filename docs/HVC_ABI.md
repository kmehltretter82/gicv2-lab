# H2 guest call ABI

H2 uses a deliberately private call ABI. It is not SMCCC and is not intended
for Linux guests.

The AArch64 guest executes `HVC #0` with the operation in `x0` and its single
argument in `x1`. A returning call receives status zero in `x0`; all other
general-purpose registers are restored. `PASS`, `FAIL`, and `EXIT` are trace
protocol operations rather than general hypervisor services.

| `x0` | Name | `x1` | Result |
| ---: | --- | --- | --- |
| `0x100` | REPORT | raw `CurrentEL`; H2 requires `4` (EL1) | mark the guest report seen and return |
| `0x101` | PASS | magic `0x600d` | require the report and expected fault, then return |
| `0x102` | FAIL | scenario-defined | print a failure and halt |
| `0x103` | EXIT | zero | require the complete sequence, print H2 PASS, and halt |

The valid H2 sequence is REPORT, the deliberate data abort, PASS, EXIT. The
monitor rejects duplicates, reordering, unknown operations, and an early exit.
An HVC resumes at the architecturally supplied return address; the monitor
does not increment `ELR_EL2` for HVC calls.
