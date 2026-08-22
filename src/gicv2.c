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
#define GICH_VMCR_QUIESCENT_WRITE UINT32_C(0)
/* Five priority bits make 2 and 3 the minimum BPR and ABPR readbacks. */
#define GICH_VMCR_QUIESCENT_READBACK \
    ((GICV2_GICV_BPR_EXPECTED << 21) | \
     ((GICV2_GICV_BPR_EXPECTED + UINT32_C(1)) << 18))

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

#define GICH_LR_LOW_BASE \
    GICH_LR_BASE(GICV2_LOW_INTID, GICV2_LOW_PRIORITY)
#define GICH_LR_LOW_PENDING \
    GICH_LR_PENDING_ENTRY(GICV2_LOW_INTID, GICV2_LOW_PRIORITY)
#define GICH_LR_LOW_ACTIVE \
    GICH_LR_ACTIVE_ENTRY(GICV2_LOW_INTID, GICV2_LOW_PRIORITY)
#define GICH_LR_HIGH_BASE \
    GICH_LR_BASE(GICV2_HIGH_INTID, GICV2_HIGH_PRIORITY)
#define GICH_LR_HIGH_PENDING \
    GICH_LR_PENDING_ENTRY(GICV2_HIGH_INTID, GICV2_HIGH_PRIORITY)
#define GICH_LR_HIGH_ACTIVE \
    GICH_LR_ACTIVE_ENTRY(GICV2_HIGH_INTID, GICV2_HIGH_PRIORITY)

#define GICH_APR_LOW_ACTIVE  GICH_APR_ACTIVE(GICV2_LOW_PRIORITY)
#define GICH_APR_HIGH_ACTIVE GICH_APR_ACTIVE(GICV2_HIGH_PRIORITY)
#define GICH_APR_BOTH_ACTIVE \
    (GICH_APR_LOW_ACTIVE | GICH_APR_HIGH_ACTIVE)

_Static_assert(GICV2_LOW_INTID != GICV2_HIGH_INTID,
               "H4i requires distinct virtual INTIDs");
_Static_assert(GICV2_HIGH_PRIORITY < GICV2_LOW_PRIORITY,
               "H4i pending interrupt must preempt the active interrupt");
_Static_assert(GICH_VMCR_INITIAL == UINT32_C(0xf85c0201),
               "H4i VMCR encoding changed");
_Static_assert(GICH_VMCR_QUIESCENT_READBACK == UINT32_C(0x004c0000),
               "H4i quiescent VMCR readback changed");
_Static_assert(GICH_LR_LOW_PENDING == UINT32_C(0x18000032),
               "H4i low pending encoding changed");
_Static_assert(GICH_LR_LOW_ACTIVE == UINT32_C(0x28000032),
               "H4i low active encoding changed");
_Static_assert(GICH_LR_HIGH_PENDING == UINT32_C(0x12000033),
               "H4i high pending encoding changed");
_Static_assert(GICH_LR_HIGH_ACTIVE == UINT32_C(0x22000033),
               "H4i high active encoding changed");
_Static_assert(GICH_APR_LOW_ACTIVE == UINT32_C(0x00010000),
               "H4i low APR encoding changed");
_Static_assert(GICH_APR_HIGH_ACTIVE == UINT32_C(0x00000010),
               "H4i high APR encoding changed");

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
                           uint32_t hcr, uint32_t vmcr,
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

    return snapshot->hcr == hcr && snapshot->vmcr == vmcr &&
           snapshot->misr == 0 &&
           snapshot->eisr[0] == 0 && snapshot->eisr[1] == 0 &&
           snapshot->elrsr[0] ==
               (empty_mask_low() & ~valid_lr_mask) &&
           snapshot->elrsr[1] == empty_mask_high() &&
           snapshot->apr == apr;
}

static bool snapshots_equal(const struct gicv2_lr_snapshot *left,
                            const struct gicv2_lr_snapshot *right)
{
    uint32_t index;

    if (left->hcr != right->hcr || left->vmcr != right->vmcr ||
        left->misr != right->misr || left->eisr[0] != right->eisr[0] ||
        left->eisr[1] != right->eisr[1] ||
        left->elrsr[0] != right->elrsr[0] ||
        left->elrsr[1] != right->elrsr[1] ||
        left->apr != right->apr) {
        return false;
    }

    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        if (left->lr[index] != right->lr[index]) {
            return false;
        }
    }
    return true;
}

static bool empty_snapshot_valid(const struct gicv2_lr_snapshot *snapshot)
{
    static const uint32_t empty[GICV2_SNAPSHOT_LRS];

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          empty, 0, 0);
}

static bool low_pending_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_PENDING, 0, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, 0, UINT32_C(1));
}

static bool low_active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, 0, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, GICH_APR_LOW_ACTIVE, UINT32_C(1));
}

static bool saved_snapshot_valid(const struct gicv2_lr_snapshot *snapshot,
                                 uint32_t hcr)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, GICH_LR_HIGH_PENDING, 0, 0
    };

    return snapshot_valid(snapshot, hcr, GICH_VMCR_INITIAL, expected,
                          GICH_APR_LOW_ACTIVE, UINT32_C(3));
}

static bool quiescent_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    static const uint32_t empty[GICV2_SNAPSHOT_LRS];

    return snapshot_valid(snapshot, 0, GICH_VMCR_QUIESCENT_READBACK,
                          empty, 0, 0);
}

static bool both_active_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, GICH_LR_HIGH_ACTIVE, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, GICH_APR_BOTH_ACTIVE, UINT32_C(3));
}

static bool high_drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, GICH_LR_HIGH_ACTIVE, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, GICH_APR_LOW_ACTIVE, UINT32_C(3));
}

static bool high_deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, GICH_LR_HIGH_BASE, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, GICH_APR_LOW_ACTIVE, UINT32_C(1));
}

static bool low_drop_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_ACTIVE, GICH_LR_HIGH_BASE, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, 0, UINT32_C(1));
}

static bool all_deactivated_snapshot_valid(
    const struct gicv2_lr_snapshot *snapshot)
{
    const uint32_t expected[GICV2_SNAPSHOT_LRS] = {
        GICH_LR_LOW_BASE, GICH_LR_HIGH_BASE, 0, 0
    };

    return snapshot_valid(snapshot, GICH_HCR_ENABLE, GICH_VMCR_INITIAL,
                          expected, 0, 0);
}

static void save_context(const struct gicv2_lr_snapshot *snapshot,
                         struct gicv2_vcpu_context *context)
{
    uint32_t index;

    context->hcr = snapshot->hcr;
    context->vmcr = snapshot->vmcr;
    context->apr = snapshot->apr;
    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        context->lr[index] = snapshot->lr[index];
    }
}

static bool context_matches(const struct gicv2_vcpu_context *context,
                            const struct gicv2_lr_snapshot *snapshot,
                            uint32_t hcr)
{
    uint32_t index;

    if (snapshot->hcr != hcr || context->hcr != GICH_HCR_ENABLE ||
        context->vmcr != snapshot->vmcr ||
        context->apr != snapshot->apr) {
        return false;
    }
    for (index = 0; index < GICV2_SNAPSHOT_LRS; index++) {
        if (context->lr[index] != snapshot->lr[index]) {
            return false;
        }
    }
    return true;
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

bool gicv2_inject_low(struct gicv2_lr_snapshot *pending)
{
    struct gicv2_lr_snapshot before;

    capture_lrs(&before);
    if (!empty_snapshot_valid(&before)) {
        *pending = before;
        return false;
    }

    mmio_write32(PI400_GICH_BASE + GICH_LR0, GICH_LR_LOW_PENDING);
    gic_barrier();
    capture_lrs(pending);
    return low_pending_snapshot_valid(pending);
}

bool gicv2_pause_save_restore(struct gicv2_context_switch *context_switch)
{
    uint32_t index;

    context_switch->completed_steps = 0;
    capture_lrs(&context_switch->low_active);
    if (!low_active_snapshot_valid(&context_switch->low_active)) {
        return false;
    }
    context_switch->completed_steps = 1;

    mmio_write32(PI400_GICH_BASE + GICH_LR0 + 4,
                 GICH_LR_HIGH_PENDING);
    gic_barrier();
    capture_lrs(&context_switch->saved);
    if (!saved_snapshot_valid(&context_switch->saved, GICH_HCR_ENABLE)) {
        return false;
    }
    save_context(&context_switch->saved, &context_switch->context);
    if (!context_matches(&context_switch->context, &context_switch->saved,
                         GICH_HCR_ENABLE)) {
        return false;
    }
    context_switch->completed_steps = 2;

    mmio_write32(PI400_GICH_BASE + GICH_HCR, 0);
    gic_barrier();
    capture_lrs(&context_switch->disabled);
    if (!saved_snapshot_valid(&context_switch->disabled, 0) ||
        !context_matches(&context_switch->context,
                         &context_switch->disabled, 0)) {
        return false;
    }
    context_switch->completed_steps = 3;

    for (index = 0; index < implemented_lrs; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4, 0);
    }
    mmio_write32(PI400_GICH_BASE + GICH_APR, 0);
    mmio_write32(PI400_GICH_BASE + GICH_VMCR,
                 GICH_VMCR_QUIESCENT_WRITE);
    gic_barrier();
    capture_lrs(&context_switch->quiescent);
    if (!quiescent_snapshot_valid(&context_switch->quiescent)) {
        return false;
    }
    context_switch->completed_steps = 4;

    mmio_write32(PI400_GICH_BASE + GICH_VMCR,
                 context_switch->context.vmcr);
    mmio_write32(PI400_GICH_BASE + GICH_APR,
                 context_switch->context.apr);
    for (index = 0; index < implemented_lrs; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4,
                     context_switch->context.lr[index]);
    }
    gic_barrier();
    capture_lrs(&context_switch->restored_disabled);
    if (!saved_snapshot_valid(&context_switch->restored_disabled, 0) ||
        !context_matches(&context_switch->context,
                         &context_switch->restored_disabled, 0)) {
        return false;
    }
    context_switch->completed_steps = 5;

    mmio_write32(PI400_GICH_BASE + GICH_HCR,
                 context_switch->context.hcr);
    gic_barrier();
    capture_lrs(&context_switch->restored);
    if (!saved_snapshot_valid(&context_switch->restored,
                              GICH_HCR_ENABLE) ||
        !context_matches(&context_switch->context,
                         &context_switch->restored, GICH_HCR_ENABLE) ||
        !snapshots_equal(&context_switch->saved,
                         &context_switch->restored)) {
        return false;
    }
    context_switch->completed_steps = 6;
    return true;
}

bool gicv2_capture_both_active(struct gicv2_lr_snapshot *active)
{
    capture_lrs(active);
    return both_active_snapshot_valid(active);
}

bool gicv2_capture_high_drop(struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return high_drop_snapshot_valid(priority_drop);
}

bool gicv2_capture_high_deactivated(
    struct gicv2_lr_snapshot *deactivated)
{
    capture_lrs(deactivated);
    return high_deactivated_snapshot_valid(deactivated);
}

bool gicv2_capture_low_resumed(struct gicv2_lr_snapshot *resumed)
{
    capture_lrs(resumed);
    return high_deactivated_snapshot_valid(resumed);
}

bool gicv2_capture_low_drop(struct gicv2_lr_snapshot *priority_drop)
{
    capture_lrs(priority_drop);
    return low_drop_snapshot_valid(priority_drop);
}

bool gicv2_finish_context_test(struct gicv2_lr_transition *transition)
{
    uint32_t index;

    capture_lrs(&transition->before);
    transition->after = transition->before;
    if (!all_deactivated_snapshot_valid(&transition->before)) {
        return false;
    }

    for (index = 0; index < implemented_lrs; index++) {
        mmio_write32(PI400_GICH_BASE + GICH_LR0 + index * 4, 0);
    }
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
