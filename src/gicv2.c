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
#define GICH_LR_PENDING         (UINT32_C(1) << 28)
#define GICH_LR_ACTIVE          (UINT32_C(2) << 28)
#define GICH_LR_BASE(intid, priority) \
    (GICH_LR_PRIORITY(priority) | (uint32_t)(intid))
#define GICH_LR_PENDING_ENTRY(intid, priority) \
    (GICH_LR_PENDING | GICH_LR_BASE(intid, priority))
#define GICH_LR_ACTIVE_ENTRY(intid, priority) \
    (GICH_LR_ACTIVE | GICH_LR_BASE(intid, priority))
#define GICH_APR_ACTIVE(priority) \
    (UINT32_C(1) << ((uint32_t)(priority) >> 3))

static uint32_t implemented_lrs;
static bool spill_queued;
static uint32_t spill_entry;

static const uint32_t lr_base[GICV2_REFILL_DELIVERIES] = {
    GICH_LR_BASE(40, 0x20),
    GICH_LR_BASE(41, 0x40),
    GICH_LR_BASE(42, 0x60),
    GICH_LR_BASE(43, 0x80),
    GICH_LR_BASE(44, 0xa0),
};

static const uint32_t lr_pending[GICV2_REFILL_DELIVERIES] = {
    GICH_LR_PENDING_ENTRY(40, 0x20),
    GICH_LR_PENDING_ENTRY(41, 0x40),
    GICH_LR_PENDING_ENTRY(42, 0x60),
    GICH_LR_PENDING_ENTRY(43, 0x80),
    GICH_LR_PENDING_ENTRY(44, 0xa0),
};

static const uint32_t lr_active[GICV2_REFILL_DELIVERIES] = {
    GICH_LR_ACTIVE_ENTRY(40, 0x20),
    GICH_LR_ACTIVE_ENTRY(41, 0x40),
    GICH_LR_ACTIVE_ENTRY(42, 0x60),
    GICH_LR_ACTIVE_ENTRY(43, 0x80),
    GICH_LR_ACTIVE_ENTRY(44, 0xa0),
};

static const uint32_t active_apr[GICV2_REFILL_DELIVERIES] = {
    GICH_APR_ACTIVE(0x20),
    GICH_APR_ACTIVE(0x40),
    GICH_APR_ACTIVE(0x60),
    GICH_APR_ACTIVE(0x80),
    GICH_APR_ACTIVE(0xa0),
};

_Static_assert(GICH_VMCR_INITIAL == UINT32_C(0xf85c0201),
               "H4g VMCR encoding changed");
_Static_assert(GICH_LR_PENDING_ENTRY(40, 0x20) == UINT32_C(0x12000028),
               "H4g first pending encoding changed");
_Static_assert(GICH_LR_PENDING_ENTRY(43, 0x80) == UINT32_C(0x1800002b),
               "H4g fourth pending encoding changed");
_Static_assert(GICH_LR_PENDING_ENTRY(44, 0xa0) == UINT32_C(0x1a00002c),
               "H4g spill encoding changed");
_Static_assert(GICH_LR_ACTIVE_ENTRY(44, 0xa0) == UINT32_C(0x2a00002c),
               "H4g spill active encoding changed");
_Static_assert(GICH_APR_ACTIVE(0xa0) == UINT32_C(0x00100000),
               "H4g spill APR encoding changed");

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
    uint32_t index;

    snapshot->hcr = mmio_read32(PI400_GICH_BASE + GICH_HCR);
    snapshot->vmcr = mmio_read32(PI400_GICH_BASE + GICH_VMCR);
    snapshot->misr = mmio_read32(PI400_GICH_BASE + GICH_MISR);
    snapshot->eisr[0] = mmio_read32(PI400_GICH_BASE + GICH_EISR0);
    snapshot->eisr[1] = read_eisr1();
    snapshot->elrsr[0] = mmio_read32(PI400_GICH_BASE + GICH_ELRSR0);
    snapshot->elrsr[1] = read_elrsr1();
    snapshot->apr = mmio_read32(PI400_GICH_BASE + GICH_APR);
    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        snapshot->lr[index] =
            mmio_read32(PI400_GICH_BASE + GICH_LR0 + index * 4);
    }
}

static bool snapshot_valid(const struct gicv2_lr_snapshot *snapshot,
                           const uint32_t expected_lrs[GICV2_SNAPSHOT_LRS],
                           uint32_t apr,
                           uint32_t valid_lr_mask)
{
    uint32_t index;

    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        if (snapshot->lr[index] != expected_lrs[index]) {
            return false;
        }
    }

    return snapshot->hcr == GICH_HCR_ENABLE &&
           snapshot->vmcr == GICH_VMCR_INITIAL &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] ==
               (empty_mask_low() & ~valid_lr_mask) &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == apr;
}

static bool empty_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    static const uint32_t empty[GICV2_SNAPSHOT_LRS];

    return snapshot_valid(snapshot, empty, 0, 0);
}

static bool full_pending_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return snapshot_valid(snapshot, lr_pending, 0, UINT32_C(0xf));
}

static void expected_delivery_lrs(uint32_t delivery, uint32_t current,
                                  uint32_t expected[GICV2_SNAPSHOT_LRS])
{
    uint32_t slot;
    uint32_t index;

    expected[0] = delivery == 0 ? lr_pending[0] : lr_pending[4];
    for (index = 1; index < GICV2_SNAPSHOT_LRS; index++) {
        expected[index] = index < delivery ? lr_base[index] :
                                             lr_pending[index];
    }

    slot = delivery == 4 ? 0 : delivery;
    expected[slot] = current;
}

static uint32_t delivery_valid_mask(uint32_t delivery)
{
    if (delivery == 0) {
        return UINT32_C(0xf);
    }
    return UINT32_C(1) |
           (UINT32_C(0xf) & ~((UINT32_C(1) << delivery) - 1));
}

static bool delivery_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot, uint32_t delivery,
    uint32_t current, uint32_t apr, uint32_t valid_lr_mask)
{
    uint32_t expected[GICV2_SNAPSHOT_LRS];

    if (delivery >= GICV2_REFILL_DELIVERIES) {
        return false;
    }
    expected_delivery_lrs(delivery, current, expected);
    return snapshot_valid(snapshot, expected, apr, valid_lr_mask);
}

static bool active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot, uint32_t delivery)
{
    if (delivery >= GICV2_REFILL_DELIVERIES) {
        return false;
    }
    return delivery_snapshot_valid(snapshot, delivery, lr_active[delivery],
                                   active_apr[delivery],
                                   delivery_valid_mask(delivery));
}

static bool drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot, uint32_t delivery)
{
    if (delivery >= GICV2_REFILL_DELIVERIES) {
        return false;
    }
    return delivery_snapshot_valid(snapshot, delivery, lr_active[delivery],
                                   0, delivery_valid_mask(delivery));
}

static bool deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot, uint32_t delivery)
{
    uint32_t slot;
    uint32_t valid_mask;

    if (delivery >= GICV2_REFILL_DELIVERIES) {
        return false;
    }
    slot = delivery == 4 ? 0 : delivery;
    valid_mask = delivery_valid_mask(delivery) & ~(UINT32_C(1) << slot);
    return delivery_snapshot_valid(snapshot, delivery, lr_base[delivery], 0,
                                   valid_mask);
}

static bool refilled_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        lr_pending[4], lr_pending[1], lr_pending[2], lr_pending[3]
    };

    return snapshot_valid(snapshot, expected, 0, UINT32_C(0xf));
}

static void init_virtual_interface(void)
{
    uint32_t index;

    spill_queued = false;
    spill_entry = 0;
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

    if (lr_count != GICV2_SNAPSHOT_LRS ||
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

bool gicv2_inject_full_lr_set(struct gicv2_lr_snapshot *pending)
{
    struct gicv2_lr_snapshot before;
    uint32_t index;

    capture_lrs(&before);
    if (!empty_snapshot_valid(&before) || spill_queued) {
        *pending = before;
        return false;
    }

    spill_entry = lr_pending[4];
    spill_queued = true;
    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4,
                     lr_pending[index]);
    }
    gic_barrier();
    capture_lrs(pending);
    return full_pending_snapshot_valid(pending) &&
           spill_entry == lr_pending[4];
}

bool gicv2_capture_refill_active(uint32_t delivery,
                                 struct gicv2_lr_snapshot *active)
{
    capture_lrs(active);
    return active_snapshot_valid(active, delivery);
}

bool gicv2_capture_refill_drop(uint32_t delivery,
                               struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return drop_snapshot_valid(priority_drop, delivery);
}

bool gicv2_complete_refill_delivery(
    uint32_t delivery, struct gicv2_lr_transition *transition)
{
    uint32_t index;

    capture_lrs(&transition->before);
    transition->after = transition->before;
    if (!deactivated_snapshot_valid(&transition->before, delivery)) {
        return false;
    }

    if (delivery == 0) {
        if (!spill_queued || spill_entry != lr_pending[4]) {
            return false;
        }
        mmio_write32(PI400_GICH_BASE + GICH_LR0, spill_entry);
        spill_entry = 0;
        spill_queued = false;
        gic_barrier();
        capture_lrs(&transition->after);
        return refilled_snapshot_valid(&transition->after);
    }

    if (delivery + 1 != GICV2_REFILL_DELIVERIES) {
        return !spill_queued && spill_entry == 0;
    }

    if (spill_queued || spill_entry != 0) {
        return false;
    }
    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4, 0);
    }
    gic_barrier();
    capture_lrs(&transition->after);
    return empty_snapshot_valid(&transition->after);
}

uint32_t gicv2_software_queue_entry(void)
{
    return spill_queued ? spill_entry : 0;
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
