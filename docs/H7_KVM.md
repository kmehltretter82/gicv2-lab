# H7 Linux/KVM workload layer

The first H7 implementation is one fixed AArch64 EL1 guest and a Python
standard-library `/dev/kvm` runner. The guest runs unchanged under QEMU TCG
and Linux arm64 KVM with `KVM_DEV_TYPE_ARM_VGIC_V2`. It software-pends SPI 42,
acknowledges it, and completes combined EOI. No physical Pi execution is
performed by the build or QEMU commands below.

Implementation and local validation do **not** close the KVM gate. Running
the existing KVM unit test, capturing the new guest under KVM, and comparing
that capture with QEMU remain pending until Linux arm64 execution is requested.

## Architectural contract and preconditions

The rule is the GICv2 pending-to-active-to-inactive lifecycle: acknowledging
an eligible pending interrupt through IAR activates it and raises running
priority; EOIR in combined mode drops priority and deactivates it. With no
asserted external line and no second software pend, it must not be delivered
again. The guest and versioned scenario both enforce the expected values.

- One AArch64 EL1 vCPU, IRQ/FIQ/SError/debug exceptions masked, MMU and caches
  initially disabled by the backend's reset state. No guest IRQ handler,
  external interrupt source, virtual timer, or second vCPU runs concurrently.
- The guest GIC presents no Security Extensions, one CPU interface, and at
  least 64 INTIDs. KVM is configured for exactly 64. QEMU's larger implemented
  interrupt count is an excluded capability fingerprint; the initial IAR
  must still be spurious.
- Only SPI 42 is enabled and targeted at CPU 0. It is level-sensitive, but
  the input line stays deasserted: GICD_ISPENDR supplies its sole pending event.
- SPI priority is `0x40`, PMR is `0xf8`, and BPR is 2. Distributor and CPU
  interfaces enable Group 0; split EOI is disabled. Device writes are followed
  by `DSB SY`.
- Entry is at `0x40000000`, with 1 MiB of guest RAM and a reserved 16 KiB
  stack at its top. No memory slot overlaps GICD (`0x08000000`, 4 KiB), GICC
  (`0x08010000`, 8 KiB), or the console (`0x09000000`).

| Checkpoint | Required observation |
| --- | --- |
| Initial | IAR `1023`, RPR `0xff` |
| Software pending | ISPENDR1 bit 10 set, HPPIR `42` |
| Acknowledged | IAR `42`, RPR `0x40`, ISACTIVER1 bit 10 set |
| After EOIR | RPR `0xff`, pending and active bits clear |
| Final | HPPIR and a second IAR both `1023` |

The guest prints six ordered markers and sixteen compared fields. Every field
has a singleton allowed-value rule, so two backends agreeing on a forbidden
value cannot pass. Truncated output, extra markers, unexpected exceptions,
and any FAIL line are rejected by H5.

H4i directly accesses EL2 and GICH state. A normal KVM EL1 guest cannot run
that monitor or inspect KVM's List Registers. H7 therefore has its own ELF
and comparison contract; H5's same-ELF and same-scenario gates remain intact.

## Build and QEMU validation on the development host

Python 3.12 or later and LLVM's AArch64 freestanding tools are sufficient.
Each build uses a new directory and preserves its complete source snapshot,
base Git commit, working changes, tool versions, build commands, ELF, flat
image, linker map, disassembly, and hashes. The snapshot includes untracked
source files; an uncommitted tree is never represented as a clean commit.

```sh
make h5-test h6-test h7-test
make LLVM_BIN=/opt/homebrew/opt/llvm/bin h7-build H7_BUILD=build/h7-frozen
make QEMU=/path/to/qemu-system-aarch64 h7-qemu \
  H7_BUILD=build/h7-frozen H7_OUT=build/h7-qemu-20 H7_RUNS=20
python3 tools/h7_runner.py verify --bundle build/h7-frozen
```

The QEMU capture copies the entire frozen bundle into its result directory.
Use that preserved bundle when transferring the payload to an approved Linux
arm64 test host. Rebuilding it changes the ELF hash and requires a fresh QEMU
baseline. To repeat a build, select a new `H7_BUILD` directory.

## Existing unit-test prerequisite

On an approved Linux arm64 host, build an unmodified
[kvm-unit-tests checkout](https://gitlab.com/kvm-unit-tests/kvm-unit-tests)
for arm64, then run the existing single-vCPU `gicv2-mmio-up` test:

```sh
python3 tools/h7_runner.py unit-tests \
  --checkout /path/to/kvm-unit-tests --compiler /path/to/compiler \
  --qemu /path/to/qemu-system-aarch64 --out results/h7-existing-kvm-tests
```

This explicitly selects `ACCEL=kvm`, captures the selected test's logs, source
archive, exact `gic.flat`, configuration, QEMU identity, compiler version, and
host/kernel identity. A skip or empty selection does not satisfy the
prerequisite. `--accel tcg` permits a separate emulator check, but its result
cannot satisfy the KVM prerequisite. The selected name and accelerator
environment follow the upstream
[test configuration](https://gitlab.com/kvm-unit-tests/kvm-unit-tests/-/blob/master/arm/unittests.cfg)
and [test runner](https://gitlab.com/kvm-unit-tests/kvm-unit-tests/-/blob/master/run_tests.sh).

## KVM capture and H5 comparison

These commands execute KVM and are reserved for a separately requested Linux
arm64 run. The runner never uses SSH, deploys a boot image, or reboots a board.

```sh
python3 tools/h7_runner.py capture-kvm \
  --bundle /path/to/the/frozen/bundle \
  --unit-tests results/h7-existing-kvm-tests --out results/h7-kvm
python3 tools/h5_runner.py compare \
  --scenario results/h7-kvm/bundle/scenario.json \
  --left /path/to/h7-qemu-20/qemu/run-001/normalized.json \
  --right results/h7-kvm/normalized.json --out results/h7-comparison.json
```

The KVM runner checks API version 12 and required capabilities, creates one
vCPU, initializes it with the preferred target, configures VGICv2 addresses
and interrupt count, then initializes the VGIC. It reads and writes back
GICD_IIDR before other userspace distributor-register accesses. These steps
follow the [Linux VGICv2 device API](https://docs.kernel.org/virt/kvm/devices/arm-vgic.html)
and [KVM API](https://docs.kernel.org/virt/kvm/api.html).

KVM owns GIC emulation. Userspace handles only the guest's PL011 data writes
and transmit-ready reads; every other MMIO exit fails. A supervising process
bounds guest execution to ten seconds by default and retains serial output,
stderr, the command, host/kernel identity, prerequisite evidence, and the
complete build bundle. Unsupported hosts return status 77 without opening
`/dev/kvm`. `check-host` only checks the OS and device's presence; it does not
prove VGICv2 support.

`h7/kvm_abi_check.c` provides compile-time checks for the ioctl numbers,
register IDs, and shared-memory offsets used by `tools/kvm_arm64.py`. It can
be checked on Linux with `cc -fsyntax-only h7/kvm_abi_check.c`, or cross-checked
on this Mac using Clang and exported Linux arm64 UAPI headers.

## Exit gate and later work

Local validation evidence is recorded in
`results/2026-09-05-h7-local/`: the frozen build, QEMU repeat captures, host
tests, and Linux UAPI compile check. This result has no KVM capture.

The first H7 gate requires the existing GICv2 KVM test to pass, a preserved
KVM capture to pass the guest's contract, and an H5 comparison to match the
QEMU capture of the identical ELF. Repeatability on KVM is additional evidence;
local mock tests or QEMU results do not establish KVM behavior.

Linux guest boot, guest PSCI, virtual timer workloads, and a device tree remain
conditional extensions. None is needed for this focused interrupt test.
