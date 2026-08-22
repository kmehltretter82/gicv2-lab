# H5 qemu-pi4 trace-runner result — 2026-08-22

## Outcome

The H5 QEMU-only host-tool baseline passed from a clean `gicv2-lab` source
revision. The runner captured a lossless raw UART trace, pinned the exact ELF,
scenario, QEMU binary, command line, stderr, and raw trace hashes, then
projected the selected H1/H2/H4i state into `gicv2-lab.trace.v1`.

Twenty fresh QEMU TCG processes ran the same H4i ELF and
`h4i-context-save-restore-v1` scenario. Run 1 was compared with runs 2 through
20. All nineteen `gicv2-lab.comparison.v1` records reported `matches: true`.
All twenty raw serial files also had the same SHA-256. A separate
manifest-based replay of run 1 produced a second matching comparison.

The pre-existing strict H4i UART oracle also passed once against the freshly
built ELF. The seven host-tool unit tests passed, including numeric-width
normalization, a one-sided forbidden-outcome negative control, a same-ELF
provenance rejection, explicit TCG command pinning, and trace-prefix
reduction.

This result establishes QEMU-side capture/replay/comparison determinism for
the fixed H4i scenario. It does **not** compare QEMU with Pi 400 hardware or
Linux KVM, and it creates no QEMU bug candidate. No Pi 400 boot, network,
storage, USB, PCIe, firmware, or KVM action was performed.

## H5 implementation and scenario

- Lab source revision: `2f0bea750192c96b2196f3a3e1e739f9cb2874d3`
- Source worktree: clean before the final build and gate
- Runner: `tools/h5_runner.py`, `RUNNER_VERSION=1`
- Scenario: `scenarios/h4i-context-save-restore-v1.json`
- Scenario SHA-256:
  `54e8d2ad32f29350a15d6159c2e026e854cefc3222d32b5732efb8284ce3e2e9`
- Scenario ID: `h4i-context-save-restore-v1`

The scenario selects only the ordered H1/H2/H4i markers and GICv2 semantic
state. It deliberately excludes host/backend fingerprints such as CPU ID,
timer rate, physical addresses, GIC implementer IDs, and trace timing. The
quiescent VMCR field is an explicit allowed-set rule; all other selected fields
compare exactly. The runner refuses to compare different scenario hashes or
different ELF hashes.

## Lab image

- ELF: `build/h5-final/gicv2-lab.elf`, 200368 bytes
- ELF SHA-256:
  `0bef6082deb828d5b423bc5e081965f05bfa601eaced8dc853bee554541ddec2`
- Raw image: `build/h5-final/gicv2-lab.bin`, 1577472 bytes
- Raw-image SHA-256:
  `9241a235f93402cebddbe6bced4f4fdf121b0f05bb9ba6913db66008a63e73cb`

The image hash is identical to the frozen H4i image because H5 adds host-side
tooling and no monitor or guest source change.

## System under test

- Backend: QEMU TCG, explicitly selected with `-accel tcg`
- Machine: `raspi400`
- CPU: `cortex-a72,has_el3=off`
- QEMU source revision: `8460833e53a91458fd3ba63ff19fb6c4932e8bb4`
- QEMU version: `11.1.0 (v0.1.0-bootstrap-18-g8460833e53-dirty)`
- QEMU binary SHA-256:
  `bf3e2f36b451af4225d3fd71eabe8981dd8ea05558977825a355690f21426c16`

The binary hash is the authoritative emulator identity. Its `-dirty` version
suffix is retained because the binary predates the later fork-local
documentation commit; those documentation changes do not alter this binary.

Each repeat process used this command, with the serial path changed only for
its distinct result directory:

```sh
../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64 \
  -machine raspi400 -accel tcg -cpu cortex-a72,has_el3=off \
  -kernel build/h5-final/gicv2-lab.elf \
  -display none -monitor none \
  -serial file:build/h5-final/repeat-20/run-001/serial.log \
  -no-reboot
```

## Host and toolchain

- Clang/LLD/LLVM tools: Homebrew LLVM 22.1.8
- Host: macOS 14.8.7 (23J520), Darwin 23.6.0, arm64
- Date and timezone: 2026-08-22, Europe/Berlin

## Gate commands

```sh
make h5-test
make LLVM_BIN=/opt/homebrew/opt/llvm/bin BUILD=build/h5-final -j4 all
make LLVM_BIN=/opt/homebrew/opt/llvm/bin BUILD=build/h5-final h5-repeat \
  H5_RUNS=20 H5_REPEAT_OUT=build/h5-final/repeat-20
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py replay-qemu \
  --run build/h5-final/repeat-20/run-001/run.json \
  --out build/h5-final/replay --timeout 10
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py compare \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --left build/h5-final/repeat-20/run-001/normalized.json \
  --right build/h5-final/replay/normalized.json \
  --out build/h5-final/replay-vs-run-001.json
SMOKE_QUIET=1 QEMU=../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64 \
  scripts/smoke-qemu.sh build/h5-final/gicv2-lab.elf
```

## Preserved evidence

`run-001.json` records the exact first capture command, QEMU version and
binary hash, scenario and image hashes, and raw serial/stderr hashes.
`repeat.json` records twenty raw-trace hashes and the nineteen successful
comparisons. `replay-vs-run-001.json` records the successful pinned replay
comparison. All twenty raw serial SHA-256 values were:

```
ecf96261e7a9e7ed5c553bdbed0d4f7f56b78637a89db12df9e8bd166fabf457
```

`serial.log.b64` losslessly preserves run 1's 11695-byte CRLF serial trace.
Its decoded SHA-256 is the value above. `qemu.stderr.b64` preserves the
81-byte runner-termination diagnostic; its decoded SHA-256 is:

```
8ab33f18f988535b6bb32a9889c726d203b70e3e793da15b1b537f444ad8d07d
```

The QEMU runner terminates the process after it observes the PASS marker, so
the preserved stderr diagnostic is expected harness cleanup, not a guest
reset or test failure.

## Remaining boundary

The trace-prefix reducer is intentionally an evidence reducer: it retains the
shortest canonical prefix containing a difference, but does not yet delete
guest operations and claim a shorter executable reproducer. A later
data-driven H5 extension can build operation-level delta reduction on top of
this recorded format. Cross-backend comparison requires separately approved H6
Pi 400 serial evidence and/or H7 KVM evidence.
