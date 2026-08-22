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
#define H4B_PRIMARY_PRIORITY UINT32_C(0x20)
#define H4B_RESERVE_PRIORITY GICV2_GICV_PMR_EXPECTED
#define GICD_MAINTENANCE_PRIORITY_OFFSET \
    (GICD_IPRIORITYR + (GICV2_MAINTENANCE_INTID & ~UINT32_C(3)))
#define GICD_MAINTENANCE_PRIORITY_SHIFT \
    ((GICV2_MAINTENANCE_INTID & UINT32_C(3)) * 8)
#define GICD_MAINTENANCE_CONFIG_SHIFT \
    ((GICV2_MAINTENANCE_INTID - 16) * 2)

#define GIC_INTERFACE_ENABLE UINT32_C(1)
#define GIC_PRIORITY_MASK    UINT32_C(0xff)
#define GIC_BINARY_POINT     UINT32_C(7)

#define GICH_HCR_ENABLE           UINT32_C(1)
#define GICH_HCR_UNDERFLOW_ENABLE (UINT32_C(1) << 1)
#define GICH_HCR_UNDERFLOW_ARMED \
    (GICH_HCR_ENABLE | GICH_HCR_UNDERFLOW_ENABLE)
#define GICH_MISR_UNDERFLOW (UINT32_C(1) << 1)

#define GICH_VMCR_ENABLE_GRP0 UINT32_C(1)
#define GICH_VMCR_VIRTUAL_ABPR (UINT32_C(7) << 18)
#define GICH_VMCR_VIRTUAL_BPR  (UINT32_C(7) << 21)
#define GICH_VMCR_PRIORITY_MASK (UINT32_C(0x1f) << 27)
#define GICH_VMCR_INITIAL \
    (GICH_VMCR_PRIORITY_MASK | GICH_VMCR_VIRTUAL_BPR | \
     GICH_VMCR_VIRTUAL_ABPR | GICH_VMCR_ENABLE_GRP0)

#define GICH_LR_PRIORITY(value) (((value) >> 3) << 23)
#define GICH_LR_PENDING         (UINT32_C(1) << 28)
#define GICH_LR_ACTIVE          (UINT32_C(2) << 28)
#define GICH_LR_PRIMARY_BASE \
    (GICH_LR_PRIORITY(H4B_PRIMARY_PRIORITY) | GICV2_PRIMARY_INTID)
#define GICH_LR_PRIMARY_PENDING \
    (GICH_LR_PENDING | GICH_LR_PRIMARY_BASE)
#define GICH_LR_PRIMARY_ACTIVE \
    (GICH_LR_ACTIVE | GICH_LR_PRIMARY_BASE)
#define GICH_LR_PRIMARY_POST_EOI GICH_LR_PRIMARY_BASE
#define GICH_LR_RESERVE_PENDING \
    (GICH_LR_PENDING | GICH_LR_PRIORITY(H4B_RESERVE_PRIORITY) | \
     GICV2_RESERVE_INTID)
#define GICH_APR_PRIMARY_ACTIVE UINT32_C(1)

static uint32_t implemented_lrs;

_Static_assert(GICV2_PRIMARY_INTID != GICV2_RESERVE_INTID,
               "H4b requires two distinct virtual INTIDs");
_Static_assert(H4B_RESERVE_PRIORITY == GICV2_GICV_PMR_EXPECTED,
               "the H4b reserve LR must be masked by GICV_PMR");

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

void gicv2_capture_lrs(struct gicv2_lr_snapshot *snapshot)
{
    snapshot->hcr = mmio_read32(PI400_GICH_BASE + GICH_HCR);
    snapshot->vmcr = mmio_read32(PI400_GICH_BASE + GICH_VMCR);
    snapshot->misr = mmio_read32(PI400_GICH_BASE + GICH_MISR);
    snapshot->eisr[0] = mmio_read32(PI400_GICH_BASE + GICH_EISR0);
    snapshot->eisr[1] = read_eisr1();
    snapshot->elrsr[0] = mmio_read32(PI400_GICH_BASE + GICH_ELRSR0);
    snapshot->elrsr[1] = read_elrsr1();
    snapshot->apr = mmio_read32(PI400_GICH_BASE + GICH_APR);
    snapshot->lr[0] = mmio_read32(PI400_GICH_BASE + GICH_LR0);
    snapshot->lr[1] = mmio_read32(PI400_GICH_BASE + GICH_LR0 + 4);
}

static bool snapshot_control_valid(const struct gicv2_lr_snapshot *snapshot,
                                   uint32_t expected_hcr)
{
    return snapshot->hcr == expected_hcr &&
           snapshot->vmcr == GICH_VMCR_INITIAL;
}

static bool empty_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_control_valid(snapshot, GICH_HCR_ENABLE) &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] == empty_mask_low() &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == 0 && snapshot->lr[0] == 0 &&
           snapshot->lr[1] == 0;
}

static bool armed_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_control_valid(snapshot, GICH_HCR_UNDERFLOW_ARMED) &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] == (empty_mask_low() & ~UINT32_C(3)) &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == 0 &&
           snapshot->lr[0] == GICH_LR_PRIMARY_PENDING &&
           snapshot->lr[1] == GICH_LR_RESERVE_PENDING;
}

bool gicv2_active_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_control_valid(snapshot, GICH_HCR_UNDERFLOW_ARMED) &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] == (empty_mask_low() & ~UINT32_C(3)) &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == GICH_APR_PRIMARY_ACTIVE &&
           snapshot->lr[0] == GICH_LR_PRIMARY_ACTIVE &&
           snapshot->lr[1] == GICH_LR_RESERVE_PENDING;
}

static void init_virtual_interface(void)
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
}

bool gicv2_init(uint32_t lr_count, struct gicv2_lr_snapshot *initial)
{
    if (lr_count < GICV2_SNAPSHOT_LRS || lr_count > 64) {
        return false;
    }

    implemented_lrs = lr_count;
    if (!init_physical_interface()) {
        return false;
    }

    init_virtual_interface();
    gicv2_capture_lrs(initial);
    return empty_snapshot_valid(initial);
}

bool gicv2_arm_underflow(struct gicv2_lr_snapshot *armed)
{
    struct gicv2_lr_snapshot before;

    if (implemented_lrs < GICV2_SNAPSHOT_LRS) {
        return false;
    }

    gicv2_capture_lrs(&before);
    if (!empty_snapshot_valid(&before)) {
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0,
                 GICH_LR_PRIMARY_PENDING);
    mmio_write32(PI400_GICH_BASE + GICH_LR0 + 4,
                 GICH_LR_RESERVE_PENDING);
    gic_barrier();
    mmio_write32(PI400_GICH_BASE + GICH_HCR,
                 GICH_HCR_UNDERFLOW_ARMED);
    gic_barrier();
    gicv2_capture_lrs(armed);
    return armed_snapshot_valid(armed);
}

void gicv2_acknowledge_underflow(struct gicv2_maintenance_trace *trace)
{
    trace->iar = mmio_read32(PI400_GICC_BASE + GICC_IAR);
    gicv2_capture_lrs(&trace->before);

    mmio_write32(PI400_GICH_BASE + GICH_HCR, GICH_HCR_ENABLE);
    gic_barrier();
    mmio_write32(PI400_GICH_BASE + GICH_LR0, 0);
    mmio_write32(PI400_GICH_BASE + GICH_LR0 + 4, 0);
    gic_barrier();
    if ((trace->iar & UINT32_C(0x3ff)) < 1020) {
        mmio_write32(PI400_GICC_BASE + GICC_EOIR, trace->iar);
        gic_barrier();
    }

    gicv2_capture_lrs(&trace->after);
}

bool gicv2_underflow_trace_valid(
    const struct gicv2_maintenance_trace *trace)
{
    return trace->iar == GICV2_MAINTENANCE_INTID &&
           snapshot_control_valid(&trace->before,
                                  GICH_HCR_UNDERFLOW_ARMED) &&
           trace->before.misr == GICH_MISR_UNDERFLOW &&
           trace->before.eisr[0] == 0 && trace->before.eisr[1] == 0 &&
           trace->before.elrsr[0] ==
               (empty_mask_low() & ~UINT32_C(2)) &&
           trace->before.elrsr[1] == empty_mask_high() &&
           trace->before.apr == 0 &&
           trace->before.lr[0] == GICH_LR_PRIMARY_POST_EOI &&
           trace->before.lr[1] == GICH_LR_RESERVE_PENDING &&
           empty_snapshot_valid(&trace->after);
}
