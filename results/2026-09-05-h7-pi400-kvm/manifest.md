# H7 Pi 400 KVM gate, 2026-09-05

The H7 exit gate passed on the physical Pi 400. The existing upstream
`gicv2-mmio-up` prerequisite passed all 17 checks under KVM on the same
host/kernel, then 20 fresh executions of the frozen H7 ELF passed its SPI
lifecycle contract and matched the QEMU TCG baseline. Every capture contains
six ordered markers and sixteen architectural fields.

## Contract and host

The architectural rule and all preconditions are in the bundle's frozen
`source/docs/H7_KVM.md` and `scenario.json`: one EL1 vCPU, one software-pended
SPI 42, no asserted external line, masked exceptions, and combined EOI.
Acknowledge must activate the interrupt and raise running priority; EOI must
drop priority and deactivate it, with no second delivery.

The host was `pi400-64`, a Raspberry Pi 400 Rev 1.0 with a Cortex-A72 and
GIC-400, reached at `karl@192.168.1.15` over Wi-Fi. It ran Linux
`6.18.39+rpt-rpi-v8`, build `#1 SMP PREEMPT Debian 1:6.18.39-1+rpt1
(2026-07-29)`, with Python 3.13.5. Test processes ran as `karl` with the
existing `kvm` group through `sudo runuser`; account memberships were not
changed. The guest console was captured through KVM MMIO exits, independently
of the physical UART.

## Preserved artifacts

`h7-kvm-evidence.tar.xz` contains the passing prerequisite and all twenty
`h7-kvm/run-NNN` directories. Every run retains its raw serial and stderr,
normalized trace, QEMU comparison, command and host identity, full frozen
guest/source bundle, and prerequisite evidence. Identical captures remain
separate files. The archive also includes the native Linux UAPI compile-check
log. Its SHA-256 is
`2e61e57da54bee594df7504d3b5fd0d7841e1cc9a9622f38ef8485c55b0dd7e2`.

The guest ELF SHA-256 is
`0b25bfcc1d42d71a459c41ad1fce0570a78953f3bf8a6d9cb993146c290a1207`;
the scenario SHA-256 is
`d7ba750715b723a0b23693fd40f2720f5521226a77df078f372b5803db572ec1`.
This is exactly the image from `../2026-09-05-h7-local/qemu-campaign.tar.gz`,
whose twenty upstream QEMU TCG captures are the comparison baseline. The
guest was built with LLVM/LLD 22.1.8. The frozen build records base commit
`0cd1cce793e867c75a6fdd058258432342a0603f` plus its complete dirty source
snapshot and diff; those changes were subsequently committed as `06d07da`.
The historical build is not relabelled as a clean build of that later commit.

The prerequisite used unmodified upstream `kvm-unit-tests` commit
`f20b0b8d3373ab489ef3ed9f2ebdf6839bcddd9c`, built with GCC 14.2.0 and run with
Debian QEMU 10.0.11, explicitly selecting `ACCEL=kvm`, one vCPU and GICv2.
Its source archive, exact `gic.flat`, configuration, compiler/QEMU identities,
commands and logs are retained. The preceding manual prerequisite run is
separately preserved in `../2026-09-05-h7-pi400-prerequisite/`.

`remote-build-kut.sh` and its log preserve the build. `remote-h7-first.sh`,
`remote-h7-repeat.sh` and their logs preserve the hardware commands and
comparison invocations. The first script also compiled
`h7/kvm_abi_check.c` against this host's native Linux headers with
`cc -fsyntax-only -Wall -Wextra -Werror`; it exited successfully with no output.

## Audit and negative control

The downloaded gzip archive matched the SHA-256 reported on the Pi,
`ce48f0d1e41870e664c527e5847cf00197d08f2e08306ed923f438a3b7541c52`.
It was recompressed with xz from 22,709,065 to 844,040 bytes; SHA-256 checks
confirmed that the complete uncompressed tar byte stream is identical.
`archive-verification.json` records both hashes and that check. Every file,
including all separate identical captures, is preserved.

The archive was extracted into a fresh local directory; every frozen bundle and prerequisite
artifact was hash-checked, every raw trace was normalized again, and all
twenty saved comparisons were reproduced against the QEMU baseline.
`validation.json` records the results per run.

All twenty raw KVM traces are 781 bytes with SHA-256
`a586195cb78ac35a1eb0a61809b8a45a46fe63a644a2c810c21a4522141e1026`.
`serial-run-001.log` and `comparison-qemu-run-001.json` provide convenient
loose copies. The supervisor intentionally terminates the halted guest after
PASS; its worker exit status is therefore `-15`, with empty stderr.

`negative-control/` is a synthetic local copy of run 001 with only
`GuestIAR_active` changed from 42 to 43. The comparator rejected it. It is
labelled synthetic and is not a hardware observation.

The initial `sudo runuser` invocation discarded the caller's Python
no-bytecode environment setting. Each preserved run bundle therefore contains
three generated Python 3.13 cache files outside its original build inventory.
`remote-h7-cache-audit.sh` compared every cached code object with compilation
of its hash-pinned source; all 60 matched. Their hashes also match the copies
in the downloaded archive. `cache-validation.json` preserves this audit.
The prepared H8 commands use `python3 -B` after `runuser` to avoid creating
those extra files.

## Kernel provenance collected after the boot campaign

`host-kernel-provenance.tar.xz` preserves the exact vendor boot kernel,
configuration, package identities, running-kernel notes, command line and
compiler/firmware metadata, collected at 07:01:17 UTC on the same host and
kernel release. This metadata was collected after H6 returned the board to
Linux; it is not represented as a pre-run snapshot.

The kernel package and source-package version are `linux`,
`1:6.18.39-1+rpt1`; the precise source Git commit has not been established.
The boot image matches `/boot/vmlinuz-6.18.39+rpt-rpi-v8`, and the installed
kernel package verification produced no differences. The image SHA-256 is
`fc341c15f1df9727f8dff25dc5acbbd872cc58ccf5d035a47ea8f6cecb07c5d8`.
The archive SHA-256 is
`dbeb9f9ad27bd94ff0c97a67c1e8e8eb248d5caae4c097c3d03aedf1a721e7b9`,
verified against the Pi's reported hash after downloading. The collection
script and log are preserved here.

The upstream unit-test runner deletes its temporary environment initrd;
that temporary file is not in the archive. Its main test image, source and
host environment are preserved. These results establish the stated fixed
scenario on this host/kernel, without extending the contract to arbitrary
interrupt workloads or Linux guest boot.
