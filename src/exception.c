/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdint.h>

#include "gicv2_lab/exception.h"
#include "gicv2_lab/gicv2.h"
#include "gicv2_lab/gicv2_defs.h"
#include "gicv2_lab/hvc.h"
#include "gicv2_lab/layout.h"
#include "gicv2_lab/platform.h"
#include "gicv2_lab/print.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK  UINT64_C(0x3f)
#define ESR_ISS_MASK UINT64_C(0x01ffffff)
#define ESR_FSC_MASK UINT64_C(0x3f)
#define ESR_HVC_IMM_MASK UINT64_C(0xffff)

#define ESR_DABT_WNR   (UINT64_C(1) << 6)
#define ESR_DABT_S1PTW (UINT64_C(1) << 7)
#define ESR_DABT_FNV   (UINT64_C(1) << 10)

#define ESR_EC_HVC64      UINT64_C(0x16)
#define ESR_EC_DABT_LOWER UINT64_C(0x24)
#define ESR_EC_BRK64      UINT64_C(0x3c)

#define VECTOR_CURRENT_SPX_SYNC UINT64_C(4)
#define VECTOR_CURRENT_SPX_IRQ  UINT64_C(5)
#define VECTOR_LOWER_A64_SYNC   UINT64_C(8)
#define VECTOR_LOWER_A64_IRQ    UINT64_C(9)
#define GUEST_CURRENTEL_VALUE   UINT64_C(4)
#define S2_LEVEL2_TRANSLATION_FAULT UINT64_C(6)

#define IRQ_READY_PMR_SHIFT 32
#define IRQ_READY_BPR_SHIFT 40
#define IRQ_READY_ALLOWED_MASK \
    (UINT64_C(0xffffffff) | (UINT64_C(0xff) << IRQ_READY_PMR_SHIFT) | \
     (UINT64_C(7) << IRQ_READY_BPR_SHIFT))

#define DUMP_SNAPSHOT(snapshot, suffix) do { \
    lab_kv_hex64("GICH_HCR_" suffix, (snapshot)->hcr); \
    lab_kv_hex64("GICH_VMCR_" suffix, (snapshot)->vmcr); \
    lab_kv_hex64("GICH_MISR_" suffix, (snapshot)->misr); \
    lab_kv_hex64("GICH_EISR0_" suffix, (snapshot)->eisr[0]); \
    lab_kv_hex64("GICH_EISR1_" suffix, (snapshot)->eisr[1]); \
    lab_kv_hex64("GICH_ELRSR0_" suffix, (snapshot)->elrsr[0]); \
    lab_kv_hex64("GICH_ELRSR1_" suffix, (snapshot)->elrsr[1]); \
    lab_kv_hex64("GICH_APR_" suffix, (snapshot)->apr); \
    lab_kv_hex64("GICH_LR0_" suffix, (snapshot)->lr[0]); \
    lab_kv_hex64("GICH_LR1_" suffix, (snapshot)->lr[1]); \
} while (0)

static bool guest_report_seen;
static bool stage2_fault_seen;
static bool h2_pass_seen;
static bool irq_ready_seen;
static bool low_active_seen;
static bool high_active_seen;
static bool high_eoi_seen;
static bool low_eoi_seen;
static bool el2_brk_seen;

_Static_assert(sizeof(struct exception_frame) == 256,
               "exception frame must match vectors.S");

static void dump_exception(const struct exception_frame *frame, uint64_t esr)
{
    lab_kv_dec("vector_slot", (uint32_t)frame->vector_slot);
    lab_kv_hex64("ESR_EL2", esr);
    lab_kv_hex64("FAR_EL2", read_far_el2());
    lab_kv_hex64("HPFAR_EL2", read_hpfar_el2());
    lab_kv_hex64("ELR_EL2", read_elr_el2());
    lab_kv_hex64("SPSR_EL2", read_spsr_el2());
}

static uint64_t fail_exception(const struct exception_frame *frame,
                               uint64_t esr, const char *reason)
{
    lab_puts("[gicv2-lab] H4c FAIL: ");
    lab_puts(reason);
    lab_puts("\n");
    dump_exception(frame, esr);
    return EXCEPTION_HALT;
}

static bool irq_ready_argument_valid(uint64_t argument)
{
    uint32_t ctlr = (uint32_t)argument;
    uint32_t pmr = (uint32_t)(argument >> IRQ_READY_PMR_SHIFT) & 0xff;
    uint32_t bpr = (uint32_t)(argument >> IRQ_READY_BPR_SHIFT) & 7;

    return (argument & ~IRQ_READY_ALLOWED_MASK) == 0 &&
           ctlr == GICV2_GICV_CTLR_EXPECTED &&
           pmr == GICV2_GICV_PMR_EXPECTED &&
           bpr == GICV2_GICV_BPR_EXPECTED;
}

static uint64_t handle_hvc(struct exception_frame *frame, uint64_t esr)
{
    uint64_t operation = frame->x[0];
    uint64_t argument = frame->x[1];

    switch (operation) {
    case HVC_REPORT:
        if (guest_report_seen || argument != GUEST_CURRENTEL_VALUE) {
            return fail_exception(frame, esr, "invalid guest report");
        }
        guest_report_seen = true;
        lab_puts("[gicv2-lab] EL1 guest report\n");
        lab_kv_dec("GuestCurrentEL", (uint32_t)(argument >> 2));
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_PASS:
        if (!guest_report_seen || !stage2_fault_seen || h2_pass_seen ||
            argument != HVC_PASS_MAGIC) {
            return fail_exception(frame, esr, "invalid guest pass");
        }
        h2_pass_seen = true;
        lab_puts("[gicv2-lab] EL1 guest resumed after stage-2 fault\n");
        lab_puts("[gicv2-lab] H2 PASS\n");
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_FAIL:
        lab_kv_hex64("guest_failure_argument", argument);
        return fail_exception(frame, esr, "guest reported failure");

    case HVC_IRQ_READY: {
        struct gicv2_lr_snapshot pending;

        if (!h2_pass_seen || irq_ready_seen ||
            !irq_ready_argument_valid(argument) ||
            !gicv2_inject_low(&pending)) {
            return fail_exception(frame, esr, "invalid IRQ-ready report");
        }
        irq_ready_seen = true;
        lab_puts("[gicv2-lab] EL1 virtual interface ready\n");
        lab_kv_hex64("GuestGICV_CTLR", (uint32_t)argument);
        lab_kv_hex64("GuestGICV_PMR",
                     (argument >> IRQ_READY_PMR_SHIFT) & 0xff);
        lab_kv_hex64("GuestGICV_BPR",
                     (argument >> IRQ_READY_BPR_SHIFT) & 7);
        lab_puts("[gicv2-lab] low-priority virtual IRQ pending\n");
        DUMP_SNAPSHOT(&pending, "low_pending");
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_IRQ_ACTIVE: {
        struct gicv2_lr_transition injection;
        bool valid;

        if (!irq_ready_seen || low_active_seen ||
            argument != GICV2_LOW_INTID) {
            return fail_exception(frame, esr,
                                  "invalid low IRQ active report");
        }

        valid = gicv2_inject_high(&injection);
        lab_puts("[gicv2-lab] low-priority virtual IRQ active\n");
        lab_kv_dec("GuestIAR_low_active", (uint32_t)argument);
        DUMP_SNAPSHOT(&injection.before, "low_active");
        lab_puts("[gicv2-lab] high-priority virtual IRQ pending\n");
        DUMP_SNAPSHOT(&injection.after, "high_pending");
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid low-active/high-pending state");
        }

        low_active_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_IRQ_NESTED_ACTIVE: {
        struct gicv2_lr_snapshot active;
        bool valid;

        if (!low_active_seen || high_active_seen || high_eoi_seen ||
            argument != GICV2_HIGH_INTID) {
            return fail_exception(frame, esr,
                                  "invalid nested IRQ active report");
        }

        valid = gicv2_capture_both_active(&active);
        lab_puts("[gicv2-lab] nested high-priority virtual IRQ active\n");
        lab_kv_dec("GuestIAR_high_active", (uint32_t)argument);
        DUMP_SNAPSHOT(&active, "both_active");
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid both-active state");
        }

        high_active_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_IRQ_NESTED_EOI: {
        struct gicv2_lr_snapshot eoi;
        bool valid;

        if (!high_active_seen || high_eoi_seen ||
            argument != GICV2_HIGH_INTID) {
            return fail_exception(frame, esr,
                                  "invalid nested IRQ EOI report");
        }

        valid = gicv2_capture_high_eoi(&eoi);
        lab_puts("[gicv2-lab] high-priority virtual IRQ EOI\n");
        lab_kv_dec("GuestIAR_high_eoi", (uint32_t)argument);
        DUMP_SNAPSHOT(&eoi, "high_eoi");
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid high-EOI state");
        }

        high_eoi_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_IRQ_EOI: {
        struct gicv2_lr_transition completion;
        bool valid;

        if (!high_eoi_seen || low_eoi_seen ||
            argument != GICV2_LOW_INTID) {
            return fail_exception(frame, esr, "invalid low IRQ EOI");
        }

        valid = gicv2_finish_priority_test(&completion);
        lab_puts("[gicv2-lab] low-priority virtual IRQ EOI\n");
        lab_kv_dec("GuestIAR_low_eoi", (uint32_t)argument);
        DUMP_SNAPSHOT(&completion.before, "low_eoi");
        lab_puts("[gicv2-lab] virtual interface restored\n");
        DUMP_SNAPSHOT(&completion.after, "cleared");
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid low-EOI/clear state");
        }

        low_eoi_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_EXIT:
        if (!guest_report_seen || !stage2_fault_seen || !h2_pass_seen ||
            !irq_ready_seen || !low_active_seen || !high_active_seen ||
            !high_eoi_seen || !low_eoi_seen || argument != 0) {
            return fail_exception(frame, esr, "guest exited too early");
        }
        lab_puts("[gicv2-lab] H4c PASS\n");
        return EXCEPTION_HALT;

    default:
        return fail_exception(frame, esr, "unknown HVC operation");
    }
}

static uint64_t handle_physical_irq(struct exception_frame *frame)
{
    uint32_t iar = gicv2_acknowledge_physical_irq();

    lab_puts("[gicv2-lab] unexpected physical IRQ\n");
    lab_kv_dec("physical_irq_vector_slot", (uint32_t)frame->vector_slot);
    lab_kv_dec("PhysicalIAR_unexpected", iar & UINT32_C(0x3ff));
    lab_puts("[gicv2-lab] H4c FAIL: physical IRQ is forbidden\n");
    return EXCEPTION_HALT;
}

uint64_t exception_dispatch(struct exception_frame *frame)
{
    uint64_t esr;
    uint64_t ec;
    uint64_t iss;
    uint64_t elr;

    if (frame->vector_slot == VECTOR_CURRENT_SPX_IRQ ||
        frame->vector_slot == VECTOR_LOWER_A64_IRQ) {
        return handle_physical_irq(frame);
    }

    esr = read_esr_el2();
    ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
    iss = esr & ESR_ISS_MASK;
    elr = read_elr_el2();

    if (frame->vector_slot == VECTOR_CURRENT_SPX_SYNC &&
        ec == ESR_EC_BRK64 && !el2_brk_seen &&
        (iss & UINT64_C(0xffff)) == UINT64_C(0x471)) {
        el2_brk_seen = true;
        lab_puts("[gicv2-lab] expected EL2 BRK\n");
        dump_exception(frame, esr);
        write_elr_el2(elr + 4);
        return EXCEPTION_RESUME;
    }

    if (frame->vector_slot == VECTOR_LOWER_A64_SYNC && ec == ESR_EC_HVC64) {
        if ((iss & ESR_HVC_IMM_MASK) != 0) {
            return fail_exception(frame, esr, "nonzero HVC immediate");
        }
        return handle_hvc(frame, esr);
    }

    if (frame->vector_slot == VECTOR_LOWER_A64_SYNC &&
        ec == ESR_EC_DABT_LOWER) {
        uint64_t fsc = iss & ESR_FSC_MASK;

        if (!guest_report_seen || stage2_fault_seen ||
            read_far_el2() != MONITOR_LOAD_BASE ||
            (iss & (ESR_DABT_WNR | ESR_DABT_S1PTW | ESR_DABT_FNV)) != 0 ||
            fsc != S2_LEVEL2_TRANSLATION_FAULT) {
            return fail_exception(frame, esr, "unexpected data abort");
        }

        stage2_fault_seen = true;
        lab_puts("[gicv2-lab] expected stage-2 translation fault\n");
        dump_exception(frame, esr);
        lab_kv_dec("stage2_fsc", (uint32_t)fsc);
        write_elr_el2(elr + 4);
        return EXCEPTION_RESUME;
    }

    return fail_exception(frame, esr, "unexpected exception");
}
