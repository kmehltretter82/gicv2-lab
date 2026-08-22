/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_GICV2_DEFS_H
#define GICV2_LAB_GICV2_DEFS_H

#define PI400_GICD_BASE 0xff841000
#define PI400_GICC_BASE 0xff842000
#define PI400_GICH_BASE 0xff844000
#define PI400_GICV_BASE 0xff846000

#define PI400_GICV_BASE_LO 0x6000
#define PI400_GICV_BASE_HI 0xff84

#define GICD_CTLR        0x000
#define GICD_TYPER       0x004
#define GICD_IIDR        0x008
#define GICD_ISENABLER0  0x100
#define GICD_ICENABLER0  0x180
#define GICD_ICPENDR0    0x280
#define GICD_ICACTIVER0  0x380
#define GICD_IPRIORITYR  0x400
#define GICD_ICFGR1      0xc04

#define GICC_CTLR 0x000
#define GICC_PMR  0x004
#define GICC_BPR  0x008
#define GICC_IAR  0x00c
#define GICC_EOIR 0x010
#define GICC_IIDR 0x0fc

#define GICH_HCR    0x000
#define GICH_VTR    0x004
#define GICH_VMCR   0x008
#define GICH_MISR   0x010
#define GICH_EISR0  0x020
#define GICH_EISR1  0x024
#define GICH_ELRSR0 0x030
#define GICH_ELRSR1 0x034
#define GICH_APR    0x0f0
#define GICH_LR0    0x100

#define GICV_CTLR 0x000
#define GICV_PMR  0x004
#define GICV_IAR  0x00c
#define GICV_EOIR 0x010

#define GICV2_MAINTENANCE_INTID 25
#define GICV2_PRIMARY_INTID     42
#define GICV2_RESERVE_INTID     43
#define GICV2_GICV_CTLR_EXPECTED 0x01
#define GICV2_GICV_PMR_EXPECTED  0xf8

#endif
