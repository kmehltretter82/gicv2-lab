/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdint.h>

#include "gicv2_lab/gicv2.h"
#include "gicv2_lab/gicv2_defs.h"
#include "gicv2_lab/platform.h"

#define GICD_PPI_MASK UINT32_C(0xffff0000)
#define GICD_MAINTENANCE_BIT \
    (UINT32_C(1) << GICV2_MAINTENANCE_INTID)
#define GICD_MAINTENANCE_PRIORITY UINT32_C(0x20)
#define H3_VIRTUAL_PRIORITY UINT32_C(0x20)
#define GICD_MAINTENANCE_PRIORITY_OFFSET \
    (GICD_IPRIORITYR + (GICV2_MAINTENANCE_INTID & ~UINT32_C(3)))
#define GICD_MAINTENANCE_PRIORITY_SHIFT \
    ((GICV2_MAINTENANCE_INTID & UINT32_C(3)) * 8)
#define GICD_MAINTENANCE_CONFIG_SHIFT \
    ((GICV2_MAINTENANCE_INTID - 16) * 2)

#define GIC_INTERFACE_ENABLE UINT32_C(1)
#define GIC_PRIORITY_MASK    UINT32_C(0xff)
#define GIC_BINARY_POINT     UINT32_C(7)

#define GICH_HCR_ENABLE UINT32_C(1)
#define GICH_MISR_EOI   UINT32_C(1)

#define GICH_VMCR_ENABLE_GRP0 UINT32_C(1)
#define GICH_VMCR_VIRTUAL_ABPR (UINT32_C(7) << 18)
#define GICH_VMCR_VIRTUAL_BPR  (UINT32_C(7) << 21)
#define GICH_VMCR_PRIORITY_MASK (UINT32_C(0x1f) << 27)
#define GICH_VMCR_INITIAL \
    (GICH_VMCR_PRIORITY_MASK | GICH_VMCR_VIRTUAL_BPR | \
     GICH_VMCR_VIRTUAL_ABPR | GICH_VMCR_ENABLE_GRP0)

#define GICH_LR_EOI          (UINT32_C(1) << 19)
#define GICH_LR_PRIORITY     ((H3_VIRTUAL_PRIORITY >> 3) << 23)
#define GICH_LR_PENDING      (UINT32_C(1) << 28)
#define GICH_LR_TEST_PENDING \
    (GICH_LR_PENDING | GICH_LR_PRIORITY | GICH_LR_EOI | \
     H3_VIRTUAL_INTID)
#define GICH_LR_TEST_POST_EOI \
    (GICH_LR_PRIORITY | GICH_LR_EOI | H3_VIRTUAL_INTID)

static uint32_t implemented_lrs;

static void gic_barrier(void)
{
    __asm__ volatile("dsb sy\n"
                     "isb\n"
                     : : : "memory");
}

static uint32_t empty_mask_low(void)
{
    if (implemented_lrs >= 32) {
        return UINT32_MAX;
    }
    return (UINT32_C(1) << implemented_lrs) - 1;
}

static uint32_t empty_mask_high(void)
{
    if (implemented_lrs <= 32) {
        return 0;
    }
    if (implemented_lrs == 64) {
        return UINT32_MAX;
    }
    return (UINT32_C(1) << (implemented_lrs - 32)) - 1;
}

static uint32_t read_eisr1(void)
{
    if (implemented_lrs <= 32) {
        return 0;
    }
    return mmio_read32(PI400_GICH_BASE + GICH_EISR1);
}

static uint32_t read_elrsr1(void)
{
    if (implemented_lrs <= 32) {
        return 0;
    }
    return mmio_read32(PI400_GICH_BASE + GICH_ELRSR1);
}

static bool init_physical_interface(void)
{
    uint32_t config;
    uint32_t priority;

    mmio_write32(PI400_GICC_BASE + GICC_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_ICENABLER0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICPENDR0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICACTIVER0, GICD_PPI_MASK);

    priority = mmio_read32(PI400_GICD_BASE +
                           GICD_MAINTENANCE_PRIORITY_OFFSET);
    priority &= ~(UINT32_C(0xff) << GICD_MAINTENANCE_PRIORITY_SHIFT);
    priority |= GICD_MAINTENANCE_PRIORITY <<
                GICD_MAINTENANCE_PRIORITY_SHIFT;
    mmio_write32(PI400_GICD_BASE + GICD_MAINTENANCE_PRIORITY_OFFSET,
                 priority);

    config = mmio_read32(PI400_GICD_BASE + GICD_ICFGR1);
    config &= ~(UINT32_C(3) << GICD_MAINTENANCE_CONFIG_SHIFT);
    mmio_write32(PI400_GICD_BASE + GICD_ICFGR1, config);

    mmio_write32(PI400_GICD_BASE + GICD_ISENABLER0,
                 GICD_MAINTENANCE_BIT);
    mmio_write32(PI400_GICC_BASE + GICC_PMR, GIC_PRIORITY_MASK);
    mmio_write32(PI400_GICC_BASE + GICC_BPR, GIC_BINARY_POINT);
    mmio_write32(PI400_GICC_BASE + GICC_CTLR, GIC_INTERFACE_ENABLE);
    mmio_write32(PI400_GICD_BASE + GICD_CTLR, GIC_INTERFACE_ENABLE);
    gic_barrier();

    priority = mmio_read32(PI400_GICD_BASE +
                           GICD_MAINTENANCE_PRIORITY_OFFSET);
    config = mmio_read32(PI400_GICD_BASE + GICD_ICFGR1);

    return mmio_read32(PI400_GICC_BASE + GICC_CTLR) ==
               GIC_INTERFACE_ENABLE &&
           mmio_read32(PI400_GICD_BASE + GICD_CTLR) ==
               GIC_INTERFACE_ENABLE &&
           mmio_read32(PI400_GICC_BASE + GICC_PMR) != 0 &&
           (mmio_read32(PI400_GICD_BASE + GICD_ISENABLER0) &
            GICD_MAINTENANCE_BIT) != 0 &&
           ((priority >> GICD_MAINTENANCE_PRIORITY_SHIFT) &
            UINT32_C(0xff)) == GICD_MAINTENANCE_PRIORITY &&
           ((config >> GICD_MAINTENANCE_CONFIG_SHIFT) & UINT32_C(3)) == 0;
}

static bool init_virtual_interface(void)
{
    uint32_t index;

    mmio_write32(PI400_GICH_BASE + GICH_HCR, 0);
    for (index = 0; index < implemented_lrs; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4, 0);
    }
    mmio_write32(PI400_GICH_BASE + GICH_APR, 0);
    mmio_write32(PI400_GICH_BASE + GICH_VMCR, GICH_VMCR_INITIAL);
    mmio_write32(PI400_GICH_BASE + GICH_HCR, GICH_HCR_ENABLE);
    gic_barrier();

    return mmio_read32(PI400_GICH_BASE + GICH_HCR) == GICH_HCR_ENABLE &&
           mmio_read32(PI400_GICH_BASE + GICH_VMCR) == GICH_VMCR_INITIAL &&
           mmio_read32(PI400_GICH_BASE + GICH_MISR) == 0 &&
           mmio_read32(PI400_GICH_BASE + GICH_EISR0) == 0 &&
           read_eisr1() == 0 &&
           mmio_read32(PI400_GICH_BASE + GICH_ELRSR0) ==
               empty_mask_low() &&
           read_elrsr1() == empty_mask_high();
}

bool gicv2_init(uint32_t lr_count)
{
    if (lr_count == 0 || lr_count > 64) {
        return false;
    }

    implemented_lrs = lr_count;
    return init_physical_interface() && init_virtual_interface();
}

bool gicv2_inject_test_irq(uint32_t *lr_value)
{
    if (implemented_lrs == 0 ||
        mmio_read32(PI400_GICH_BASE + GICH_HCR) != GICH_HCR_ENABLE ||
        (mmio_read32(PI400_GICH_BASE + GICH_ELRSR0) & 1) == 0 ||
        mmio_read32(PI400_GICH_BASE + GICH_LR0) != 0) {
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0, GICH_LR_TEST_PENDING);
    gic_barrier();
    *lr_value = mmio_read32(PI400_GICH_BASE + GICH_LR0);

    return *lr_value == GICH_LR_TEST_PENDING &&
           (mmio_read32(PI400_GICH_BASE + GICH_ELRSR0) & 1) == 0 &&
           mmio_read32(PI400_GICH_BASE + GICH_MISR) == 0;
}

void gicv2_acknowledge_maintenance(struct gicv2_maintenance_trace *trace)
{
    trace->iar = mmio_read32(PI400_GICC_BASE + GICC_IAR);
    trace->hcr = mmio_read32(PI400_GICH_BASE + GICH_HCR);
    trace->misr = mmio_read32(PI400_GICH_BASE + GICH_MISR);
    trace->eisr[0] = mmio_read32(PI400_GICH_BASE + GICH_EISR0);
    trace->eisr[1] = read_eisr1();
    trace->elrsr[0] = mmio_read32(PI400_GICH_BASE + GICH_ELRSR0);
    trace->elrsr[1] = read_elrsr1();
    trace->apr = mmio_read32(PI400_GICH_BASE + GICH_APR);
    trace->lr0 = mmio_read32(PI400_GICH_BASE + GICH_LR0);

    mmio_write32(PI400_GICH_BASE + GICH_LR0, 0);
    gic_barrier();
    if ((trace->iar & UINT32_C(0x3ff)) < 1020) {
        mmio_write32(PI400_GICC_BASE + GICC_EOIR, trace->iar);
        gic_barrier();
    }

    trace->misr_after = mmio_read32(PI400_GICH_BASE + GICH_MISR);
    trace->eisr_after[0] = mmio_read32(PI400_GICH_BASE + GICH_EISR0);
    trace->eisr_after[1] = read_eisr1();
    trace->elrsr_after[0] = mmio_read32(PI400_GICH_BASE + GICH_ELRSR0);
    trace->elrsr_after[1] = read_elrsr1();
    trace->lr0_after = mmio_read32(PI400_GICH_BASE + GICH_LR0);
}

bool gicv2_maintenance_trace_valid(
    const struct gicv2_maintenance_trace *trace)
{
    return trace->iar == GICV2_MAINTENANCE_INTID &&
           trace->hcr == GICH_HCR_ENABLE &&
           trace->misr == GICH_MISR_EOI &&
           trace->eisr[0] == UINT32_C(1) && trace->eisr[1] == 0 &&
           trace->elrsr[0] == (empty_mask_low() & ~UINT32_C(1)) &&
           trace->elrsr[1] == empty_mask_high() && trace->apr == 0 &&
           trace->lr0 == GICH_LR_TEST_POST_EOI &&
           trace->misr_after == 0 &&
           trace->eisr_after[0] == 0 && trace->eisr_after[1] == 0 &&
           trace->elrsr_after[0] == empty_mask_low() &&
           trace->elrsr_after[1] == empty_mask_high() &&
           trace->lr0_after == 0;
}
