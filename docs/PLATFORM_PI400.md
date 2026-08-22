# Pi 400 platform facts used by H1 through H3

The Pi 400 uses BCM2711 with four Cortex-A72 cores and a GIC-400 implementing
the GICv2 virtualization extensions.

The EL2 monitor uses only these peripheral addresses with its MMU disabled:

| Block | Address | Current use |
| --- | ---: | --- |
| PL011 UART0 | `0xfe201000` | serial diagnostic output |
| GICD | `0xff841000` | configure and enable maintenance PPI 25 |
| GICC | `0xff842000` | acknowledge and EOI physical maintenance IRQs |
| GICH | `0xff844000` | configure virtualization state and List Registers |
| GICV | `0xff846000` | one 4 KiB page exposed to the EL1 guest |

The GIC distributor, physical CPU interface, hypervisor interface, and virtual
CPU interface are separate frames. H1 only reads GICH_VTR, and H2 does not
initialize the GIC. H3 initializes the physical and virtual interfaces and
injects one software virtual interrupt through LR0. The GIC virtualization
maintenance output is PPI number 9, architectural INTID 25.

EL2 accesses all four physical frames directly with its MMU disabled. Stage 2
allows EL1 to access only the GICV page; the guest cannot program GICD, GICC,
or GICH.

The production Pi boot chain must enter this lab in non-secure EL2. The QEMU
smoke test explicitly disables its optional EL3 model so that the test begins
in the same usable exception level.

References:

- https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
- https://developer.arm.com/documentation/ihi0048/latest/
- https://developer.arm.com/compute-ip/corelink-gic-400
- https://www.kernel.org/doc/html/latest/virt/kvm/devices/arm-vgic.html
