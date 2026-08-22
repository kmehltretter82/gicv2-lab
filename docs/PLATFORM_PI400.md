# Pi 400 platform facts used by H1 through H4i

The Pi 400 uses BCM2711 with four Cortex-A72 cores and a GIC-400 implementing
the GICv2 virtualization extensions.

The EL2 monitor uses only these peripheral addresses with its MMU disabled:

| Block | Address | Current use |
| --- | ---: | --- |
| PL011 UART0 | `0xfe201000` | serial diagnostic output |
| GICD | `0xff841000` | configure maintenance in H3-H4b; quiesce PPIs in H4c-H4g and H4i; enable timer PPI 26 in H4h |
| GICC | `0xff842000` | service maintenance in H3-H4b and timer PPI 26 in H4h; remain quiescent in H4i |
| GICH | `0xff844000` | configure virtualization state and List Registers |
| GICV | `0xff846000` | exact 8 KiB register block exposed to EL1 in H4d-H4i |

The GIC distributor, physical CPU interface, hypervisor interface, and virtual
CPU interface are separate frames. H1 only reads GICH_VTR, and H2 does not
initialize the GIC. H3 initializes the physical and virtual interfaces and
injects one software virtual interrupt through LR0. H4a and H4b isolate LR
lifecycle and underflow-maintenance behavior. H4c disables all maintenance
causes and PPIs while it uses LR0 and LR1 for nested priority preemption. The
H4d through H4g also disable every maintenance cause and PPI. H4d distinguishes
the priority drop at `GICV_EOIR` from deactivation at `GICV_DIR`; H4e uses
that split to expose Pending again from a Pending+Active LR; H4f preserves
two distinct software SGI source tags through HPPIR, IAR, EOIR, and DIR. H4g
fills all four qemu-pi4 LRs and refills one empty slot from an EL2 software
queue. H4h enables only non-secure EL2 physical timer PPI number 10,
architectural INTID 26, and uses `CNTHP_EL2` to drive the wake. The GIC
virtualization maintenance output used by the earlier scenarios is PPI
number 9, architectural INTID 25.

H4i again disables every PPI and maintenance cause. It uses GICH only to save,
disable, quiesce, and restore the single vCPU's HCR, VMCR, APR, and four LRs.
No physical interrupt participates in the context switch or the subsequent
nested virtual delivery.

EL2 accesses all four physical frames directly with its MMU disabled. The
H4d-H4i stage 2 allows EL1 to access only the architectural 8 KiB GICV block:
the first page contains IAR, EOIR, RPR, and HPPIR, and the second contains DIR
at offset `0x1000`. The guest cannot program GICD, GICC, GICH, or adjacent
MMIO.

The production Pi boot chain must enter this lab in non-secure EL2. The QEMU
smoke test explicitly disables its optional EL3 model so that the test begins
in the same usable exception level.

References:

- https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
- https://developer.arm.com/documentation/ihi0048/latest/
- https://developer.arm.com/compute-ip/corelink-gic-400
- https://www.kernel.org/doc/html/latest/virt/kvm/devices/arm-vgic.html
