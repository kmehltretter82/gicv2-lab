# H6 host preparation — 2026-09-05

The updated H6 host tooling prepared and verified a fresh source/image/QEMU
bundle. Its status is **ready, zero hardware attempts**, with a target of 100.
The strict H4i QEMU smoke oracle also passed against this bundle's ELF;
`smoke-serial.log`, `smoke-stderr.log`, and `smoke.json` preserve that check.

This result performs no hardware access and does not extend or close the
historical 36-boot hardware campaign.

- Base source commit: `0cd1cce793e867c75a6fdd058258432342a0603f`, with uncommitted working files
  preserved in `source/` and the tracked changes in `source.diff`.
- Toolchain: Homebrew clang version 22.1.8.
- ELF SHA-256: `e9615fb39589d21eb7eec7d9ff77723b9e670655bceca843fd62887edb2f019c`.
- Flat image SHA-256: `bf78cbdcd4af2008d342a7c85da5040c6c36b4fe6bccf83b54cff4a7216a4950`.
- Archive SHA-256: `4e50472a92775e86384d4cfa172daabe249116a3d5ddd7db10b9e24158e3bb2a`.

`prepared-gate.tar.gz` retains all source, image, build, environment, and QEMU
baseline artifacts. The archive was extracted into a different directory and
verified successfully; `verification.json` records the initial gate state.
The 11 H6 host tests cover evidence retention, strict resumption, shared
resource locks, deployed image selection, and relocatable archives. Hardware
validation of the new capture driver remains pending.

The historical result has changed ELF provenance and incomplete archived raw
captures. Start a fresh campaign when physical execution is requested. Build
and archive handling, deployment requirements, and the opt-in driver command
are documented in `docs/H6_PI400_BOOT.md`.

Later on 2026-09-05, a fresh extraction of this bundle completed the full
100-boot hardware gate. That separate result and successful boot-file cleanup
are preserved in [the hardware manifest](../2026-09-05-h6-pi400-100/manifest.md).
This preparation archive still contains zero hardware attempts.
