/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdint.h>

#include "gicv2_lab/gicv2.h"
#include "gicv2_lab/gicv2_defs.h"
#include "gicv2_lab/platform.h"

#define GICD_PPI_MASK UINT32_C(0xffff0000)

#define GIC_INTERFACE_ENABLE UINT32_C(1)
#define GIC_PRIORITY_MASK    UINT32_C(0xff)
#define GIC_BINARY_POINT     UINT32_C(7)

#define GICH_VTR_LIST_REGS_MASK UINT32_C(0x3f)
#define GICH_VTR_PREBITS_SHIFT  26
#define GICH_VTR_PRIBITS_SHIFT  29
#define GICH_VTR_BITS_MASK      UINT32_C(7)
#define GICH_VTR_FIVE_BITS      UINT32_C(4)

#define GICH_HCR_ENABLE UINT32_C(1)

#define GICH_VMCR_ENABLE_GRP0  UINT32_C(1)
#define GICH_VMCR_EOI_SPLIT    (UINT32_C(1) << 9)
#define GICH_VMCR_VIRTUAL_ABPR (UINT32_C(7) << 18)
#define GICH_VMCR_VIRTUAL_BPR \
    ((uint32_t)GICV2_GICV_BPR_EXPECTED << 21)
#define GICH_VMCR_PRIORITY_MASK (UINT32_C(0x1f) << 27)
#define GICH_VMCR_INITIAL \
    (GICH_VMCR_PRIORITY_MASK | GICH_VMCR_VIRTUAL_BPR | \
     GICH_VMCR_VIRTUAL_ABPR | GICH_VMCR_EOI_SPLIT | \
     GICH_VMCR_ENABLE_GRP0)

#define GICH_LR_PRIORITY(value) (((uint32_t)(value) >> 3) << 23)
#define GICH_LR_CPUID(value)    ((uint32_t)(value) << 10)
#define GICH_LR_PENDING         (UINT32_C(1) << 28)
#define GICH_LR_ACTIVE          (UINT32_C(2) << 28)
#define GICH_LR_FIRST_BASE \
    (GICH_LR_PRIORITY(GICV2_FIRST_SGI_PRIORITY) | \
     GICH_LR_CPUID(GICV2_FIRST_SGI_CPUID) | GICV2_FIRST_SGI_INTID)
#define GICH_LR_SECOND_BASE \
    (GICH_LR_PRIORITY(GICV2_SECOND_SGI_PRIORITY) | \
     GICH_LR_CPUID(GICV2_SECOND_SGI_CPUID) | GICV2_SECOND_SGI_INTID)
#define GICH_LR_FIRST_PENDING  (GICH_LR_PENDING | GICH_LR_FIRST_BASE)
#define GICH_LR_FIRST_ACTIVE   (GICH_LR_ACTIVE | GICH_LR_FIRST_BASE)
#define GICH_LR_SECOND_PENDING (GICH_LR_PENDING | GICH_LR_SECOND_BASE)
#define GICH_LR_SECOND_ACTIVE  (GICH_LR_ACTIVE | GICH_LR_SECOND_BASE)

#define GICH_APR_FIRST_ACTIVE \
    (UINT32_C(1) << (GICV2_FIRST_SGI_PRIORITY >> 3))
#define GICH_APR_SECOND_ACTIVE \
    (UINT32_C(1) << (GICV2_SECOND_SGI_PRIORITY >> 3))

static uint32_t implemented_lrs;

_Static_assert(GICH_VMCR_INITIAL == UINT32_C(0xf85c0201),
               "H4f VMCR encoding changed");
_Static_assert(GICH_LR_FIRST_PENDING == UINT32_C(0x12000405),
               "H4f first pending SGI encoding changed");
_Static_assert(GICH_LR_FIRST_ACTIVE == UINT32_C(0x22000405),
               "H4f first active SGI encoding changed");
_Static_assert(GICH_LR_SECOND_PENDING == UINT32_C(0x18000c06),
               "H4f second pending SGI encoding changed");
_Static_assert(GICH_LR_SECOND_ACTIVE == UINT32_C(0x28000c06),
               "H4f second active SGI encoding changed");
_Static_assert(GICH_APR_FIRST_ACTIVE == UINT32_C(0x00000010),
               "H4f first APR encoding changed");
_Static_assert(GICH_APR_SECOND_ACTIVE == UINT32_C(0x00010000),
               "H4f second APR encoding changed");
_Static_assert(GICV2_FIRST_SGI_IAR == UINT32_C(0x00000405),
               "H4f first IAR encoding changed");
_Static_assert(GICV2_SECOND_SGI_IAR == UINT32_C(0x00000c06),
               "H4f second IAR encoding changed");

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
    mmio_write32(PI400_GICC_BASE + GICC_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_ICENABLER0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICPENDR0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICACTIVER0, GICD_PPI_MASK);

    mmio_write32(PI400_GICC_BASE + GICC_PMR, GIC_PRIORITY_MASK);
    mmio_write32(PI400_GICC_BASE + GICC_BPR, GIC_BINARY_POINT);
    mmio_write32(PI400_GICC_BASE + GICC_CTLR, GIC_INTERFACE_ENABLE);
    mmio_write32(PI400_GICD_BASE + GICD_CTLR, GIC_INTERFACE_ENABLE);
    gic_barrier();

    return mmio_read32(PI400_GICC_BASE + GICC_CTLR) ==
               GIC_INTERFACE_ENABLE &&
           mmio_read32(PI400_GICD_BASE + GICD_CTLR) ==
               GIC_INTERFACE_ENABLE &&
           mmio_read32(PI400_GICC_BASE + GICC_PMR) != 0 &&
           (mmio_read32(PI400_GICD_BASE + GICD_ISENABLER0) &
            GICD_PPI_MASK) == 0;
}

static void capture_lrs(struct gicv2_lr_snapshot *snapshot)
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

static bool snapshot_valid(const struct gicv2_lr_snapshot *snapshot,
                           uint32_t lr0, uint32_t lr1, uint32_t apr,
                           uint32_t valid_lr_mask)
{
    return snapshot->hcr == GICH_HCR_ENABLE &&
           snapshot->vmcr == GICH_VMCR_INITIAL &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] ==
               (empty_mask_low() & ~valid_lr_mask) &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == apr && snapshot->lr[0] == lr0 &&
           snapshot->lr[1] == lr1;
}

static bool empty_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, 0, 0, 0, 0);
}

static bool both_pending_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_PENDING,
                          GICH_LR_SECOND_PENDING, 0, UINT32_C(3));
}

static bool first_active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_ACTIVE,
                          GICH_LR_SECOND_PENDING,
                          GICH_APR_FIRST_ACTIVE, UINT32_C(3));
}

static bool first_drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_ACTIVE,
                          GICH_LR_SECOND_PENDING, 0, UINT32_C(3));
}

static bool first_deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_BASE,
                          GICH_LR_SECOND_PENDING, 0, UINT32_C(2));
}

static bool second_active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_BASE,
                          GICH_LR_SECOND_ACTIVE,
                          GICH_APR_SECOND_ACTIVE, UINT32_C(2));
}

static bool second_drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_BASE,
                          GICH_LR_SECOND_ACTIVE, 0, UINT32_C(2));
}

static bool both_deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, GICH_LR_FIRST_BASE,
                          GICH_LR_SECOND_BASE, 0, 0);
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

bool gicv2_init(uint32_t gich_vtr, struct gicv2_lr_snapshot *initial)
{
    uint32_t lr_count = (gich_vtr & GICH_VTR_LIST_REGS_MASK) + 1;
    uint32_t prebits =
        (gich_vtr >> GICH_VTR_PREBITS_SHIFT) & GICH_VTR_BITS_MASK;
    uint32_t pribits =
        (gich_vtr >> GICH_VTR_PRIBITS_SHIFT) & GICH_VTR_BITS_MASK;

    if (lr_count < GICV2_SNAPSHOT_LRS || lr_count > 64 ||
        prebits != GICH_VTR_FIVE_BITS ||
        pribits != GICH_VTR_FIVE_BITS) {
        return false;
    }

    implemented_lrs = lr_count;
    if (!init_physical_interface()) {
        return false;
    }

    init_virtual_interface();
    capture_lrs(initial);
    return empty_snapshot_valid(initial);
}

bool gicv2_inject_sgis(struct gicv2_lr_snapshot *pending)
{
    struct gicv2_lr_snapshot before;

    capture_lrs(&before);
    if (!empty_snapshot_valid(&before)) {
        *pending = before;
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0, GICH_LR_FIRST_PENDING);
    mmio_write32(PI400_GICH_BASE + GICH_LR0 + 4,
                 GICH_LR_SECOND_PENDING);
    gic_barrier();
    capture_lrs(pending);
    return both_pending_snapshot_valid(pending);
}

bool gicv2_capture_first_sgi_active(struct gicv2_lr_snapshot *active)
{
    capture_lrs(active);
    return first_active_snapshot_valid(active);
}

bool gicv2_capture_first_sgi_drop(
    struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return first_drop_snapshot_valid(priority_drop);
}

bool gicv2_capture_first_sgi_deactivated(
    struct gicv2_lr_snapshot *deactivated)
{
    capture_lrs(deactivated);
    return first_deactivated_snapshot_valid(deactivated);
}

bool gicv2_capture_second_sgi_active(struct gicv2_lr_snapshot *active)
{
    capture_lrs(active);
    return second_active_snapshot_valid(active);
}

bool gicv2_capture_second_sgi_drop(
    struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return second_drop_snapshot_valid(priority_drop);
}

bool gicv2_finish_sgis(struct gicv2_lr_transition *transition)
{
    capture_lrs(&transition->before);
    transition->after = transition->before;
    if (!both_deactivated_snapshot_valid(&transition->before)) {
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0, 0);
    mmio_write32(PI400_GICH_BASE + GICH_LR0 + 4, 0);
    gic_barrier();
    capture_lrs(&transition->after);
    return empty_snapshot_valid(&transition->after);
}

uint32_t gicv2_acknowledge_physical_irq(void)
{
    uint32_t iar = mmio_read32(PI400_GICC_BASE + GICC_IAR);

    if ((iar & UINT32_C(0x3ff)) < 1020) {
        mmio_write32(PI400_GICC_BASE + GICC_EOIR, iar);
        gic_barrier();
    }
    return iar;
}
