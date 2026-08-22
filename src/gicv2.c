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
static bool hyp_timer_armed;
static bool hyp_timer_expired;
static uint64_t hyp_timer_start;
static uint64_t hyp_timer_delay;

#define HYP_TIMER_PPI_MASK \
    (UINT32_C(1) << GICV2_HYP_TIMER_INTID)
#define HYP_TIMER_PRIORITY UINT32_C(0x20)
#define HYP_TIMER_PRIORITY_SHIFT \
    ((GICV2_HYP_TIMER_INTID & UINT32_C(3)) * 8)
#define HYP_TIMER_CONFIG_SHIFT \
    ((GICV2_HYP_TIMER_INTID - 16) * 2)
#define HYP_TIMER_EDGE_BIT \
    (UINT32_C(2) << HYP_TIMER_CONFIG_SHIFT)

#define GICH_LR_WAKE_BASE \
    GICH_LR_BASE(GICV2_WAKE_INTID, GICV2_WAKE_PRIORITY)
#define GICH_LR_WAKE_PENDING \
    GICH_LR_PENDING_ENTRY(GICV2_WAKE_INTID, GICV2_WAKE_PRIORITY)
#define GICH_LR_WAKE_ACTIVE \
    GICH_LR_ACTIVE_ENTRY(GICV2_WAKE_INTID, GICV2_WAKE_PRIORITY)
#define GICH_APR_WAKE_ACTIVE GICH_APR_ACTIVE(GICV2_WAKE_PRIORITY)

_Static_assert(GICH_VMCR_INITIAL == UINT32_C(0xf85c0201),
               "H4h VMCR encoding changed");
_Static_assert(GICH_LR_WAKE_PENDING == UINT32_C(0x14000030),
               "H4h pending wake encoding changed");
_Static_assert(GICH_LR_WAKE_ACTIVE == UINT32_C(0x24000030),
               "H4h active wake encoding changed");
_Static_assert(GICH_LR_WAKE_BASE == UINT32_C(0x04000030),
               "H4h invalid wake encoding changed");
_Static_assert(GICH_APR_WAKE_ACTIVE == UINT32_C(0x00000100),
               "H4h wake APR encoding changed");
_Static_assert(HYP_TIMER_PPI_MASK == UINT32_C(0x04000000),
               "H4h timer PPI mask changed");

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

    write_cnthp_ctl_el2(0);
    mmio_write32(PI400_GICC_BASE + GICC_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_CTLR, 0);
    mmio_write32(PI400_GICD_BASE + GICD_ICENABLER0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICPENDR0, GICD_PPI_MASK);
    mmio_write32(PI400_GICD_BASE + GICD_ICACTIVER0, GICD_PPI_MASK);

    priority = mmio_read32(PI400_GICD_BASE + GICD_IPRIORITYR +
                           (GICV2_HYP_TIMER_INTID & ~UINT32_C(3)));
    priority &= ~(UINT32_C(0xff) << HYP_TIMER_PRIORITY_SHIFT);
    priority |= HYP_TIMER_PRIORITY << HYP_TIMER_PRIORITY_SHIFT;
    mmio_write32(PI400_GICD_BASE + GICD_IPRIORITYR +
                 (GICV2_HYP_TIMER_INTID & ~UINT32_C(3)), priority);

    config = mmio_read32(PI400_GICD_BASE + GICD_ICFGR1);
    config &= ~HYP_TIMER_EDGE_BIT;
    mmio_write32(PI400_GICD_BASE + GICD_ICFGR1, config);
    mmio_write32(PI400_GICD_BASE + GICD_ISENABLER0,
                 HYP_TIMER_PPI_MASK);

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
            GICD_PPI_MASK) == HYP_TIMER_PPI_MASK &&
           ((mmio_read32(PI400_GICD_BASE + GICD_IPRIORITYR +
                         (GICV2_HYP_TIMER_INTID & ~UINT32_C(3))) >>
             HYP_TIMER_PRIORITY_SHIFT) & UINT32_C(0xff)) ==
               HYP_TIMER_PRIORITY &&
           (read_cnthp_ctl_el2() &
            (GICV2_CNTHP_CTL_ENABLE | GICV2_CNTHP_CTL_IMASK)) == 0;
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

static bool wake_snapshot_valid(const struct gicv2_lr_snapshot *snapshot,
                                uint32_t lr0, uint32_t apr,
                                uint32_t valid_lr_mask)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = { lr0, 0, 0, 0 };

    return snapshot_valid(snapshot, expected, apr, valid_lr_mask);
}

static bool wake_pending_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return wake_snapshot_valid(snapshot, GICH_LR_WAKE_PENDING, 0,
                               UINT32_C(1));
}

static bool wake_active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return wake_snapshot_valid(snapshot, GICH_LR_WAKE_ACTIVE,
                               GICH_APR_WAKE_ACTIVE, UINT32_C(1));
}

static bool wake_drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return wake_snapshot_valid(snapshot, GICH_LR_WAKE_ACTIVE, 0,
                               UINT32_C(1));
}

static bool wake_deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    return wake_snapshot_valid(snapshot, GICH_LR_WAKE_BASE, 0, 0);
}

static void init_virtual_interface(void)
{
    uint32_t index;

    hyp_timer_armed = false;
    hyp_timer_expired = false;
    hyp_timer_start = 0;
    hyp_timer_delay = 0;
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

bool gicv2_capture_empty(struct gicv2_lr_snapshot *empty)
{
    capture_lrs(empty);
    return empty_snapshot_valid(empty);
}

bool gicv2_arm_hyp_timer(uint64_t *delay_ticks, uint64_t *start_count,
                         uint32_t *control)
{
    struct gicv2_lr_snapshot empty;
    uint64_t frequency = read_cntfrq_el0();
    uint64_t delay = frequency / GICV2_HYP_TIMER_DIVISOR;

    if (hyp_timer_armed || hyp_timer_expired || delay == 0 ||
        delay > INT32_MAX || !gicv2_capture_empty(&empty)) {
        return false;
    }

    write_cnthp_ctl_el2(0);
    hyp_timer_start = read_cntpct_el0();
    hyp_timer_delay = delay;
    hyp_timer_armed = true;
    write_cnthp_tval_el2(delay);
    write_cnthp_ctl_el2(GICV2_CNTHP_CTL_ENABLE);
    gic_barrier();

    *delay_ticks = hyp_timer_delay;
    *start_count = hyp_timer_start;
    *control = (uint32_t)read_cnthp_ctl_el2();
    if (*control != GICV2_CNTHP_CTL_ENABLE) {
        write_cnthp_ctl_el2(0);
        hyp_timer_armed = false;
        return false;
    }
    return true;
}

bool gicv2_service_hyp_timer(uint32_t *iar, uint32_t *control,
                             uint64_t *elapsed_ticks,
                             struct gicv2_lr_snapshot *pending)
{
    struct gicv2_lr_snapshot empty;
    uint64_t now;
    bool valid;

    *iar = mmio_read32(PI400_GICC_BASE + GICC_IAR);
    *control = (uint32_t)read_cnthp_ctl_el2();
    now = read_cntpct_el0();
    *elapsed_ticks = now - hyp_timer_start;

    valid = hyp_timer_armed && !hyp_timer_expired &&
            (*iar & UINT32_C(0x3ff)) == GICV2_HYP_TIMER_INTID &&
            (*control & (GICV2_CNTHP_CTL_ENABLE |
                         GICV2_CNTHP_CTL_IMASK |
                         GICV2_CNTHP_CTL_ISTATUS)) ==
                (GICV2_CNTHP_CTL_ENABLE | GICV2_CNTHP_CTL_ISTATUS) &&
            *elapsed_ticks >= hyp_timer_delay &&
            gicv2_capture_empty(&empty);

    write_cnthp_ctl_el2(0);
    if (valid) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0,
                     GICH_LR_WAKE_PENDING);
        gic_barrier();
    }

    if ((*iar & UINT32_C(0x3ff)) < 1020) {
        mmio_write32(PI400_GICC_BASE + GICC_EOIR, *iar);
        gic_barrier();
    }

    if (!valid) {
        return false;
    }

    capture_lrs(pending);
    if (!wake_pending_snapshot_valid(pending)) {
        return false;
    }

    hyp_timer_armed = false;
    hyp_timer_expired = true;
    return true;
}

bool gicv2_hyp_timer_fired(void)
{
    return hyp_timer_expired && !hyp_timer_armed;
}

bool gicv2_capture_wake_active(struct gicv2_lr_snapshot *active)
{
    capture_lrs(active);
    return hyp_timer_expired && wake_active_snapshot_valid(active);
}

bool gicv2_capture_wake_drop(struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return hyp_timer_expired && wake_drop_snapshot_valid(priority_drop);
}

bool gicv2_finish_wake(struct gicv2_lr_transition *transition)
{
    capture_lrs(&transition->before);
    transition->after = transition->before;
    if (!hyp_timer_expired ||
        !wake_deactivated_snapshot_valid(&transition->before)) {
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0, 0);
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
