# Pi 400 KVM prerequisite, 2026-09-05

The unmodified upstream `kvm-unit-tests` `gicv2-mmio-up` test passed all
17 checks on `pi400-64`, using real KVM on the vendor
`6.18.39+rpt-rpi-v8` kernel. The test wrapper exited 0.

This is a manual prerequisite capture. At the time it was collected, the H7
guest and its prerequisite wrapper had not yet run on the Pi, and no H6 boot
had been attempted in this session. It does not close either hardware exit
gate. The subsequent approved H7 runs and completed KVM gate are recorded in
`../2026-09-05-h7-pi400-kvm/`.

The user explicitly authorized Pi 400 use over Wi-Fi. SSH confirmed a
Raspberry Pi 400 Rev 1.0 at `karl@192.168.1.15`; the Mac's serial adapter
`/dev/cu.usbserial-B0043XBS` was free. GPIO14/15 remain inputs under the normal
boot configuration. Existing `config.txt` and `tryboot.txt` were copied to
backups and their hashes verified locally; neither boot file was modified.
The frozen project payloads had not yet been uploaded at this checkpoint.

`prerequisite-evidence.tar.gz` retains the exact test image, upstream source
archive and commit, build configuration, compiler/QEMU versions and hashes,
full build commands and output, exact test invocation, raw test serial trace,
host preflight, and original boot configuration. `validation.json` records
the image and archive hashes. Every archived file was checked against its
local original after archiving.

The selected upstream source commit is
`f20b0b8d3373ab489ef3ed9f2ebdf6839bcddd9c`. The build used GCC
14.2.0 and the run used Debian QEMU 10.0.11 with `-accel kvm`, one vCPU,
and `-machine gic-version=2`. Only `arm/gic.flat` was built and only
`gicv2-mmio-up` was selected. Test processes ran as `karl` with the existing
`kvm` group via `sudo runuser`; no account membership was changed.

The raw guest's `EXIT: STATUS=1` is the upstream framework's success encoding;
its host wrapper reports `PASS gicv2-mmio-up (17 tests)` and exits 0. The
upstream runner's temporary environment initrd was removed by that runner
and is not present in this archive; the main test image and source are saved.
