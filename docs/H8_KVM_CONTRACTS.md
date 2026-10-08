# H8 deterministic KVM interrupt contracts

H8 starts with three fixed EL1 workloads, each built as a separate ELF and
versioned scenario. They use the H7 runner and the same single-vCPU platform.
The guest checks each observation immediately; H5 independently enforces
singleton allowed values, complete fields, and ordered checkpoints.

## Common preconditions

- One freshly created AArch64 EL1 vCPU. DAIF remains `0x3c0`; MMU and caches
  are off. The guest disables both architectural timers before configuring
  the GIC. It polls IAR and never enters an IRQ handler.
- GICv2 without Security Extensions, one CPU interface, at least 64 INTIDs.
  KVM exposes 64; QEMU's interrupt count is an excluded capability fingerprint.
- Group 0 is enabled in the distributor and CPU interface. Only SPI 42, or
  SPIs 42 and 43 for H8b, are enabled and targeted at CPU 0. Their input lines
  remain deasserted. Pending events come from GICD_ISPENDR1 writes.
- The selected SPIs are configured level-sensitive. This does not test an
  asserted level input or edge detection. BPR is 2; the chosen priorities
  differ in group-priority bits and are representable with five priority bits.
- PMR starts at `0xf8`. Initial IAR is `1023`, RPR is `0xff`, and the selected
  pending and active bits are clear. Each MMIO access is followed by `DSB SY`.
- The RAM, stack and MMIO layout are the [H7 layout](H7_KVM.md). The guest
  touches only the distributor, CPU interface and fixed serial console.

The architectural basis is Arm's
[GICv2 specification, IHI 0048B.b](https://documentation-service.arm.com/static/5f8ff196f86e16515cdbf969),
Chapter 3 interrupt states and priority handling, and Chapter 4 descriptions
of GICC_IAR, GICC_EOIR, GICC_RPR, GICC_HPPIR and GICC_DIR. The rules below
apply under the explicit preconditions above.

## H8a: split EOI

With EOImode set, EOIR drops running priority; DIR separately deactivates
the interrupt. Without another pending event, completing both operations
leaves no interrupt eligible for acknowledgement.

SPI 42 has priority `0x40`, and GICC_CTLR is `0x201`.

| Checkpoint | Required observation |
| --- | --- |
| Pending | Pending bit set; HPPIR 42 |
| Acknowledged | IAR 42; RPR `0x40`; active bit set; pending bit clear |
| After EOIR | RPR `0xff`; active bit retained; pending bit clear; IAR 1023 |
| After DIR | Active bit clear |
| Final | RPR `0xff`; active and pending clear; HPPIR and IAR 1023 |

`split-eoi` builds `h7/split_eoi.c` with shared setup. Its scenario is
`h8a-split-eoi-v1`: seven markers and 26 fields.

## H8b: priority mask and preemption eligibility

IAR excludes an interrupt whose priority equals PMR. Once the mask permits
delivery, an interrupt with higher group priority can be acknowledged while
the lower-priority interrupt remains active. Completing them in reverse
acknowledgement order restores the previous running priority.

SPI 42 has priority `0x80`, SPI 43 has priority `0x20`, and GICC_CTLR is 1
(combined EOI). DAIF stays masked throughout: this tests acknowledgement
eligibility and running-priority nesting, not nested CPU exception entry.

| Checkpoint | Required observation |
| --- | --- |
| SPI 42 pending, PMR `0x80` | IAR 1023; RPR `0xff`; pending retained; no active bit |
| PMR raised to `0xf8` | IAR 42; RPR `0x80`; SPI 42 active |
| SPI 43 then pended | IAR 43; RPR `0x20`; both SPIs active |
| EOIR for 43 | RPR `0x80`; only SPI 42 active |
| EOIR for 42, final | RPR `0xff`; active and pending clear; HPPIR and IAR 1023 |

HPPIR is not used as a PMR eligibility oracle: its architectural selection
does not apply the same priority-mask and running-priority checks as IAR.

`priority` builds `h7/priority.c` with shared setup. Its scenario is
`h8b-priority-v1`: eight markers and 29 fields.

## H8c: active-and-pending redelivery

A second software pending event can coexist with active state. Priority
drop retains that state; deactivation makes the pending interrupt eligible
again. A second acknowledgement consumes the remaining pending event.

SPI 42 has priority `0x40`, and GICC_CTLR is `0x201`.

| Checkpoint | Required observation |
| --- | --- |
| First acknowledgement | IAR 42; RPR `0x40`; active set; pending clear |
| Software pend while active | Active and pending set; HPPIR 1023 |
| First EOIR | RPR `0xff`; active and pending retained; IAR 1023 |
| First DIR | Active clear; pending retained; HPPIR 42 |
| Second acknowledgement | IAR 42; RPR `0x40`; active set; pending clear |
| Second EOIR | RPR `0xff`; active retained |
| Second DIR, final | Active and pending clear; HPPIR and IAR 1023 |

`redelivery` builds `h7/redelivery.c` with shared setup. Its scenario is
`h8c-redelivery-v1`: nine markers and 35 fields.

## Build, freeze and compare

Select `split-eoi`, `priority` or `redelivery`; the default remains the
original H7 `spi-lifecycle` workload. Use a fresh output directory each time.

```sh
make h5-test h6-test h7-test
make LLVM_BIN=/opt/homebrew/opt/llvm/bin h7-build \
  H7_WORKLOAD=split-eoi H7_BUILD=build/h8a-frozen
make QEMU=/path/to/qemu-system-aarch64 h7-qemu \
  H7_BUILD=build/h8a-frozen H7_OUT=build/h8a-qemu-20 H7_RUNS=20
```

The [H7 KVM capture procedure](H7_KVM.md#kvm-capture-and-h5-comparison)
accepts these bundles unchanged, including the same-host/kernel passing
`gicv2-mmio-up` prerequisite. Each capture executes the runner preserved
inside its bundle. Compare only identical ELF and scenario hashes. A failure
or difference is an observation to investigate against the contract; it does
not establish a Linux defect.

These scenarios do not cover external-line transitions, userspace state
save/restore, nested IRQ handlers, multi-vCPU routing, or different kernel
revisions. Track demonstrated execution in the [coverage matrix](KVM_COVERAGE.md).
