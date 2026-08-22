# gicv2-lab instructions

gicv2-lab is a small, deterministic AArch64 EL2 test laboratory. Its first
milestone is QEMU-only. Do not boot an image on the physical Pi 400, modify a
boot medium, or send an external report without the user's explicit request.

Keep each scenario small and auditable:

- State the architectural rule being tested and all of its preconditions.
- Treat a QEMU/Pi/KVM difference as evidence to investigate, not proof of a
  defect.
- Preserve the exact image, source commit, toolchain version, command line,
  and serial trace for every result.
- Do not turn code or text produced with AI assistance in this repository into
  a QEMU upstream patch. Follow QEMU's current code-provenance policy
  independently for any future upstream work.

The project is intentionally not a general-purpose or production hypervisor.
Avoid guest device drivers, a filesystem, networking, dynamic allocation, or
SMP until a focused test needs them.
