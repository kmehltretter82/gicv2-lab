# H8 deterministic contracts: local validation, 2026-09-05

Three fixed single-vCPU EL1 contracts passed twenty fresh QEMU TCG runs
each. All 29 host tests passed. No H8 KVM execution has occurred.

The rules, preconditions and coverage limits are in
[H8_KVM_CONTRACTS.md](../../docs/H8_KVM_CONTRACTS.md), also preserved in
each frozen bundle. These are valid-behavior checks using software-pended
SPIs with external lines deasserted and CPU exceptions masked.

| Workload | QEMU captures | Markers | Fields |
| --- | --- | --- | --- |
| priority | 20/20 | 8 | 29 |
| redelivery | 20/20 | 9 | 35 |
| split-eoi | 20/20 | 7 | 26 |

## Preserved evidence

`h8-qemu-campaign.tar.xz` contains `h8/<workload>/bundle` and all twenty
`qemu/run-NNN` captures per workload. Every bundle preserves the exact
ELF, flat image, linker map, disassembly, source snapshot, source diff,
base commit, compiler versions and build commands. Every run preserves
raw serial/stderr, normalized records, QEMU identity and command.

The source base is `06d07daddd1f326b1a48a60bfa079895aa12d8cc` plus the
complete dirty source snapshot. LLVM/LLD 22.1.8 built the guests; upstream
QEMU 11.0.2 ran the frozen images on the development Mac. No result is
represented as a clean source commit.

The complete archive is 127,820 bytes with SHA-256
`ea9fbac61d3a12162dfb3928dc6649b2450477540873c045cd7b67174ae43f79`.

| Workload | ELF SHA-256 |
| --- | --- |
| priority | `0ae6231073efb14438d7d9383703662e9e0a01d08cbb7b340a850628bb507d77` |
| redelivery | `ec0ea3b41d08976bfc7aea338940d2fa2ef56a17a95681d137814559184bd848` |
| split-eoi | `51a7b685042acdb681744affa46178094791f8d6d0cd37618d47222f2f2f7007` |

## Audit and negative controls

`audit-h8.py --qemu-only --out results/2026-09-05-h8-local`, run from the
repository root, extracted the archive into a fresh directory, verified
all frozen artifacts, re-normalized all sixty raw captures, and checked
each against the allowed-value contract and its first capture.
`validation.json` records scenario hashes and every per-run raw hash.

Each workload has a loose first trace and clearly labelled synthetic
negative controls. A key expected value was changed; comparison rejected
it both against the valid baseline and against an identically wrong trace.
Truncating the PASS marker and replacing a checkpoint out of order were
also rejected. These controls were never executed as guests.

The original default `spi-lifecycle` selection also built and passed one fresh
QEMU capture after the runner changes. `default-h7-regression.tar.xz`
preserves that separate bundle and capture; it is not part of the proposed
H8 upload payload.

## Pending KVM execution

`remote-h8-first.sh` and `remote-h8-repeat.sh` are prepared, unexecuted
commands for the Pi session. They verify the transferred archive, retain
the existing same-host/kernel KVM prerequisite, run twenty fresh captures
per workload and compare the same frozen ELF/scenario against QEMU.

Automatic approval review rejected the upload because it interpreted the
earlier explicit approval as limited to H6/H7 payloads. The H8 archive was
not transferred and the scripts were not executed. `proposed-transfer.json`
records the concrete payload and destination for approval.

No additional Linux KVM coverage or defect is claimed by this local result.
External-line handling, userspace state preservation, nested IRQ exception
entry, SMP and kernel-revision comparisons remain separate work.
