# H7 local implementation validation — 2026-09-05

The new EL1 SPI lifecycle guest passed 20 fresh unmodified-QEMU TCG processes.
All twenty traces match across six markers and sixteen architectural fields.
The synthetic negative control changes the observed IAR from 42 to 43 and is
rejected as a one-sided forbidden outcome. It is explicitly synthetic evidence.

No KVM VM, physical Pi boot, boot-medium modification, or external report was
performed. The existing GICv2 KVM unit-test prerequisite and the real KVM
capture/comparison remain pending. `host-check.txt` records status 77 on this
macOS host; that check does not open `/dev/kvm`.

## Exact implementation

- Base source commit: `0cd1cce793e867c75a6fdd058258432342a0603f`.
- The working tree was uncommitted. The complete source snapshot, including
  untracked implementation files, and tracked-file diff are inside the archive.
  The base commit alone does not identify the implementation.
- Compiler: Homebrew clang version 22.1.8.
- Guest ELF SHA-256: `0b25bfcc1d42d71a459c41ad1fce0570a78953f3bf8a6d9cb993146c290a1207`.
- Scenario SHA-256: `d7ba750715b723a0b23693fd40f2720f5521226a77df078f372b5803db572ec1`.
- QEMU: QEMU emulator version 11.0.2.
- QEMU binary SHA-256: `81bf328e9e26ee6e51bf4833dd1030ea5d40f7611a1a4c23000b2aeebce41597`.
- Guest entry: `0x40000000`; one EL1 vCPU, 1 MiB RAM, software-pended SPI 42.

`qemu-campaign.tar.gz` contains the exact ELF and flat image, source snapshot,
build commands and tool versions, map/disassembly, every raw serial/stderr
capture, all twenty run manifests and normalized traces, and nineteen
comparisons. Its SHA-256 is `e6b22403d067612ecdb3e106945fe0533dfc3f6d2fc6974832f2ab774885ea28`.
All twenty raw captures were re-normalized and compared successfully after
extracting the archive at a different path. No raw captures were deduplicated.

## Validation

- `make h5-test h6-test h7-test`: 28 tests passed (7 H5, 11 H6, 10 H7).
- ShellCheck accepted the H6 driver.
- The Linux arm64 UAPI compile check passed. `kvm-abi-check.i` preserves the
  expanded input; `abi-check.json` records commands, tool version, and input
  header hashes. This validates ABI layout, not KVM execution.
- The archived capture supervisor's timeout test preserves partial output and
  terminates its own worker. Non-Linux hosts are rejected before device access.

## Offline inspection

Extract the archive into a new directory, then run:

```sh
python3 tools/h7_runner.py verify --bundle /path/to/extracted/h7/bundle
```

The preserved bundle can be transferred to an approved Linux arm64 host later.
Use the prerequisite and capture commands in `docs/H7_KVM.md`; retain the
identical ELF. Absolute paths in historical run manifests record the original
invocation and are not required for H5 comparison of the preserved traces.
