# H5 deterministic differential runner

## Scope and safety boundary

H5 is the host-side evidence layer for the bare-metal scenarios. It captures
the exact serial trace, pins the scenario and ELF hashes, projects raw output
into a versioned semantic trace, compares two runs, and retains the shortest
trace prefix containing the first difference.

This first implementation executes **only** local QEMU TCG. It does not boot a
Pi 400, touch a boot medium, connect to a board, or invoke Linux KVM.
`import-trace` is deliberately passive: after an explicitly approved future
H6 or H7 run has produced a serial capture, it can copy that capture into an
H5 result directory and apply the same validation and provenance gates.

The runner is `tools/h5_runner.py`; its documents are JSON rather than an
unversioned collection of shell output:

| Document | Purpose |
| --- | --- |
| `gicv2-lab.scenario.v1` | Scenario input and semantic comparison contract. |
| `gicv2-lab.run.v1` | Exact image, scenario, backend, command, and raw-trace provenance. |
| `gicv2-lab.trace.v1` | Canonical sequence of selected markers and numeric state fields. |
| `gicv2-lab.comparison.v1` | Result of comparing two traces. |
| `gicv2-lab.repeat.v1` | Aggregate fresh-process repeatability result. |
| `gicv2-lab.reduction.v1` | First-difference prefix reduction record. |

Every capture uses a new empty output directory. This prevents a later run
from silently overwriting or being compared with stale evidence.

## H4i scenario contract

`scenarios/h4i-context-save-restore-v1.json` describes the currently fixed
H4i ELF scenario. It selects the ordered H1/H2/H4i markers, all H4i GICH
snapshots, saved context, and guest-visible IAR/RPR/HPPIR state. It does not
compare incidental backend fingerprints such as MIDR, MPIDR, timer frequency,
ELR/FAR addresses, page-table addresses, GIC implementer IDs, or raw trace
timing.

UART numeric printing is normalized to a lowercase `0x` integer without a
fixed width. Thus a backend cannot create a false difference merely by
printing `1` where another prints `0x0001`.

The default field rule is `equal`: both runs must have the same ordered field
and canonical value. A scenario can instead declare `allowed`, with an
explicit set of architecturally justified values. Each backend must then be
inside that set; two different allowed values are not reported as a defect.
The only current H4i allowed-set rule is the quiescent VMCR readback, whose
single value reflects the five discovered priority bits. Do not add values to
an allowed set merely to make two implementations compare cleanly: record the
controlling architectural rule and all relevant capability preconditions.

Before comparing traces, H5 requires equal scenario IDs and hashes and equal
ELF SHA-256 hashes. A payload mismatch is a provenance failure, not a QEMU
versus-hardware result. This is how H5 enforces “same payload bytes and
scenario input.”

## QEMU workflow

Run host-tool unit tests first:

```sh
make h5-test
```

Capture one fresh QEMU run. Choose a new output path each time:

```sh
make LLVM_BIN=/opt/homebrew/opt/llvm/bin h5-qemu H5_OUT=build/h5/qemu-a
```

Replay the exact image/scenario from that manifest into a new directory:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py replay-qemu \
  --run build/h5/qemu-a/run.json \
  --out build/h5/qemu-b
```

Compare the two normalized traces. Exit status zero means the selected
semantic sequence matches; a nonzero status still writes the comparison JSON:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py compare \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --left build/h5/qemu-a/normalized.json \
  --right build/h5/qemu-b/normalized.json \
  --out build/h5/qemu-a-vs-b.json
```

For a fresh-process repeatability gate, preserve every capture and compare
runs 2 through N against run 1:

```sh
make LLVM_BIN=/opt/homebrew/opt/llvm/bin h5-repeat \
  H5_RUNS=20 H5_REPEAT_OUT=build/h5/repeat-20
```

`repeat.json` names every raw trace and comparison. The raw traces may differ
in ignored diagnostics; the normalized traces must not.

## Later hardware and KVM evidence

After H6 or H7 is separately approved and an external serial file exists,
import it without performing any board action:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py import-trace \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --backend pi400 --backend-id serial-capture-label \
  --image /absolute/path/to/the-identical/gicv2-lab.elf \
  --serial /absolute/path/to/captured-serial.log \
  --out results/YYYY-MM-DD-h5-pi400
```

The image hash gate intentionally rejects a capture made with different bytes.
If an implementation requires a permitted alternate outcome, extend the
versioned scenario contract first, cite why the outcome is allowed, and retain
the original contract and traces.

## Reduction

When a comparison fails, retain the shortest prefix that contains the first
semantic difference:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/h5_runner.py reduce \
  --scenario scenarios/h4i-context-save-restore-v1.json \
  --left build/h5/qemu-a/normalized.json \
  --right build/h5/qemu-b/normalized.json \
  --out build/h5/reduced
```

This is an evidence reducer, not yet an executable-operation reducer. It
removes irrelevant trailing trace records but does not claim that deleting
guest operations would reproduce the failure. A later data-driven scenario
format can use the retained prefix as the starting point for true delta
debugging of injection operations.

## H5 QEMU-only exit gate

The H5 host-tool baseline is ready to freeze only after all of the following
are retained together:

1. `make h5-test` passes, including a one-sided forbidden-outcome negative
   control and prefix-reduction test.
2. Two fresh QEMU captures of the same H4i ELF compare equal.
3. A replay from the first run's manifest compares equal.
4. A fresh-process repeat gate passes with all raw traces, normalized traces,
   comparison JSON, run manifests, image hash, scenario hash, QEMU hash and
   version preserved.

Passing this gate establishes QEMU-side capture and comparison determinism. It
does not establish that QEMU matches a Pi 400 or KVM; that conclusion requires
separately approved H6/H7 evidence.
