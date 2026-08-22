# Pi 400 platform facts used by H1 and H2

The Pi 400 uses BCM2711 with four Cortex-A72 cores and a GIC-400 implementing
the GICv2 virtualization extensions.

The EL2 monitor uses only these peripheral addresses with its MMU disabled:

| Block | Address | Current use |
| --- | ---: | --- |
| PL011 UART0 | 0xfe201000 | serial diagnostic output |
| GICH | 0xff844000 | read GICH_VTR only |

The GIC distributor, CPU interface, GICH, and GICV frames are separate. H2
does not initialize the GIC or inject an interrupt; that starts in H3. The H2
EL1 guest has no stage-2 mapping for either peripheral and performs no MMIO.

The production Pi boot chain must enter this lab in non-secure EL2. The QEMU
smoke test explicitly disables its optional EL3 model so that the test begins
in the same usable exception level.

References:

- https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
- https://developer.arm.com/compute-ip/corelink-gic-400
- https://www.kernel.org/doc/html/latest/virt/kvm/devices/arm-vgic.html
