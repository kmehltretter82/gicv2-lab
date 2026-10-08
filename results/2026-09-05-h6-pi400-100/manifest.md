# H6 Pi 400 100-boot gate, 2026-09-05

The H6 exit gate passed: **100 consecutive fresh hardware boots**, all matching
one frozen qemu-pi4 baseline of the identical ELF across 25 ordered markers and
221 architectural fields. Every attempt passed; none was skipped or resumed.
The campaign ran from 05:27:21 to 06:58:35 UTC on 2026-09-05.

## Platform and execution

The board was a Raspberry Pi 400 Rev 1.0, BCM2711 Cortex-A72 with GIC-400,
reached as `karl@192.168.1.15` over Wi-Fi. Serial was captured from the GPIO
UART through `/dev/cu.usbserial-B0043XBS` at 115200 baud. Firmware revision
was `288930ab4712b99596f32732664aaaeb881ef1e0` (2026-05-21).
The board returned between tests to vendor kernel `6.18.39+rpt-rpi-v8`.

The user explicitly authorized physical execution and transfer of this H6
image and boot configuration. The frozen `scripts/h6-repeat.sh` driver ran
100 one-shot tryboots; the image's watchdog returned the board to Linux after
each trace. `h6-session.json`, `h6-driver.log`, deployment scripts/logs and
per-attempt `commands.sh` preserve the exact commands and environment.

`tryboot-h6.txt` selects 64-bit boot, PL011 UART, `disable-bt`, a 3 MHz UART
clock and `gicv2-lab-h6.bin`. The original `config.txt` remained unchanged.
The architectural contract and preconditions are preserved in the bundle's
`source/docs/H4I_CONTEXT_SAVE_RESTORE.md` and `scenario.json`; this hardware
campaign executes the existing H4i EL2 context-preservation workload.

## Frozen provenance

The bundle was prepared before hardware access and preserved separately in
`../2026-09-05-h6-local/prepared-gate.tar.gz`. A fresh extraction supplied the
zero-attempt gate for this campaign. The original preparation artifact was
not modified or relabelled as containing hardware results.

- ELF SHA-256: `e9615fb39589d21eb7eec7d9ff77723b9e670655bceca843fd62887edb2f019c`.
- Flat image SHA-256: `bf78cbdcd4af2008d342a7c85da5040c6c36b4fe6bccf83b54cff4a7216a4950`.
- Complete archive SHA-256: `1a5db8b46df499dd0b8215c841078e9915601fc4db78f893c7bb8a0caacebf82`.
- Base source commit: `0cd1cce793e867c75a6fdd058258432342a0603f`, plus the
  complete dirty source snapshot and diff. Those changes were subsequently
  committed as `06d07da`; the frozen build retains its original provenance.
- Toolchain: LLVM/LLD 22.1.8. Exact executable identities, versions, build and
  QEMU commands are in the archived environment and run manifests.

`complete-gate.tar.gz` contains the source, scenario, ELF, flat image, linker
map, disassembly, build environment, QEMU baseline, and all 100 complete
attempt directories. Each attempt retains original serial bytes, normalized
trace, comparison, command, board identity and provenance checks.

## Audit and cleanup

The gate verifier passed before archiving and again after extraction into a
fresh directory. Both audits reported `complete`, 100 attempts and 100 passes.
`validation.json` records per-run hashes, times and serial lengths.

Run 001 contains 11,820 raw bytes; runs 002–100 contain 11,770 bytes each.
Their only raw difference is the serial prefix before the first lab marker.
All bytes are preserved, including that prefix; all compared lab records agree.
The first raw SHA-256 is
`5608beb7a273d1aff24b6df69984585dc2814caedd221927962279b027d482d7`;
the remaining 99 share
`b875a2d55c509341fe8b9e8c7b40645053c9fa068f472cbc7f715bf5cf2375b5`.
Loose first/last traces are provided for inspection.

`negative-control/` is a synthetic copy of run 001 with CurrentEL changed
from 2 to 3. The comparator rejected it; it is not a hardware observation.

The first cleanup restored the correct `tryboot.txt` bytes but `cp -p`
returned an ownership-preservation error on the FAT boot partition, stopping
before image removal. The original controller status and error log remain
unchanged. Rerunning the idempotent restoration verified the original files
and removed the temporary image. `h6-restoration-recovery.json` and its log
record successful completion at 07:00:33 UTC. Both original boot-file hashes
match, the temporary image is absent, the vendor kernel is running, GPIO
14/15 are back to their original input state, and throttling flags are zero.

The failure was in post-campaign file cleanup; the 100 test comparisons had
already passed. No additional H6 hardware gate work remains. Further behavior
coverage belongs to separate scenarios, including the Linux KVM work.
