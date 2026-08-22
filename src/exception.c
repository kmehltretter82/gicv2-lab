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

#define IRQ_STATE_RPR_SHIFT 32
#define IRQ_STATE_HPPIR_SHIFT 40
#define IRQ_STATE_HPPIR_MASK UINT32_C(0x1fff)
#define IRQ_STATE_ALLOWED_MASK \
    (UINT64_C(0xffffffff) | (UINT64_C(0xff) << IRQ_STATE_RPR_SHIFT) | \
     ((uint64_t)IRQ_STATE_HPPIR_MASK << IRQ_STATE_HPPIR_SHIFT))

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
    lab_kv_hex64("GICH_LR2_" suffix, (snapshot)->lr[2]); \
    lab_kv_hex64("GICH_LR3_" suffix, (snapshot)->lr[3]); \
} while (0)

static bool guest_report_seen;
static bool stage2_fault_seen;
static bool h2_pass_seen;
static bool irq_ready_seen;
static bool pending_hppir_seen;
static uint32_t active_reports;
static uint32_t eoi_reports;
static uint32_t completion_reports;
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
    lab_puts("[gicv2-lab] H4g FAIL: ");
    lab_puts(reason);
    lab_puts("\n");
    dump_exception(frame, esr);
    return EXCEPTION_HALT;
}

static bool irq_state_argument_valid(uint64_t argument,
                                     uint32_t expected_iar,
                                     uint32_t expected_rpr,
                                     uint32_t expected_hppir)
{
    return (argument & ~IRQ_STATE_ALLOWED_MASK) == 0 &&
           (uint32_t)argument == expected_iar &&
           ((uint32_t)(argument >> IRQ_STATE_RPR_SHIFT) & 0xff) ==
               expected_rpr &&
           ((uint32_t)(argument >> IRQ_STATE_HPPIR_SHIFT) &
            IRQ_STATE_HPPIR_MASK) ==
               expected_hppir;
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

static void dump_irq_state(const char *iar_key, const char *rpr_key,
                           const char *hppir_key, uint64_t argument)
{
    lab_kv_hex64(iar_key, (uint32_t)argument);
    lab_kv_hex64(rpr_key,
                 (argument >> IRQ_STATE_RPR_SHIFT) & UINT64_C(0xff));
    lab_kv_hex64(hppir_key,
                 (argument >> IRQ_STATE_HPPIR_SHIFT) &
                 IRQ_STATE_HPPIR_MASK);
}

#define DUMP_REFILL_CASE(checkpoint_value, suffix) \
    case checkpoint_value: \
        lab_kv_hex64("GuestIRQState_" suffix, argument); \
        DUMP_SNAPSHOT(snapshot, suffix); \
        break

static void dump_refill_state(const struct gicv2_lr_snapshot *snapshot,
                              uint32_t delivery, uint32_t phase,
                              uint64_t argument)
{
    uint32_t checkpoint = delivery * 3 + phase;

    lab_kv_dec("RefillDelivery", delivery);
    lab_kv_dec("RefillPhase", phase);
    dump_irq_state("GuestIAR_refill", "GuestRPR_refill",
                   "GuestHPPIR_refill", argument);

    switch (checkpoint) {
        DUMP_REFILL_CASE(0, "delivery0_active");
        DUMP_REFILL_CASE(1, "delivery0_drop");
        DUMP_REFILL_CASE(2, "delivery0_deactivated");
        DUMP_REFILL_CASE(3, "delivery1_active");
        DUMP_REFILL_CASE(4, "delivery1_drop");
        DUMP_REFILL_CASE(5, "delivery1_deactivated");
        DUMP_REFILL_CASE(6, "delivery2_active");
        DUMP_REFILL_CASE(7, "delivery2_drop");
        DUMP_REFILL_CASE(8, "delivery2_deactivated");
        DUMP_REFILL_CASE(9, "delivery3_active");
        DUMP_REFILL_CASE(10, "delivery3_drop");
        DUMP_REFILL_CASE(11, "delivery3_deactivated");
        DUMP_REFILL_CASE(12, "delivery4_active");
        DUMP_REFILL_CASE(13, "delivery4_drop");
        DUMP_REFILL_CASE(14, "delivery4_deactivated");
    default:
        break;
    }
}

#undef DUMP_REFILL_CASE

static uint32_t refill_expected_iar(uint32_t delivery)
{
    return GICV2_REFILL_FIRST_INTID + delivery;
}

static uint32_t refill_expected_rpr(uint32_t delivery)
{
    return (delivery + 1) * GICV2_REFILL_PRIORITY_STEP;
}

static uint32_t refill_expected_hppir(uint32_t delivery)
{
    if (delivery + 1 == GICV2_REFILL_DELIVERIES) {
        return GICV2_GICV_HPPIR_SPURIOUS_EXPECTED;
    }
    return GICV2_REFILL_FIRST_INTID + delivery + 1;
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
            !gicv2_inject_full_lr_set(&pending) ||
            gicv2_software_queue_entry() !=
                GICV2_REFILL_SPILL_LR_EXPECTED) {
            return fail_exception(frame, esr, "invalid IRQ-ready report");
        }
        irq_ready_seen = true;
        lab_puts("[gicv2-lab] EL1 virtual interface ready\n");
        lab_kv_hex64("GuestGICV_CTLR", (uint32_t)argument);
        lab_kv_hex64("GuestGICV_PMR",
                     (argument >> IRQ_READY_PMR_SHIFT) & 0xff);
        lab_kv_hex64("GuestGICV_BPR",
                     (argument >> IRQ_READY_BPR_SHIFT) & 7);
        lab_puts("[gicv2-lab] all four List Registers occupied\n");
        DUMP_SNAPSHOT(&pending, "all_pending");
        lab_puts("[gicv2-lab] fifth virtual interrupt queued in EL2\n");
        lab_kv_hex64("EL2SoftwareQueue_entry",
                     gicv2_software_queue_entry());
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_REFILL_PENDING:
        lab_kv_hex64("GuestHPPIR_all_pending_observed", argument);
        if (!irq_ready_seen || pending_hppir_seen || active_reports != 0 ||
            eoi_reports != 0 || completion_reports != 0 ||
            argument != GICV2_REFILL_FIRST_INTID) {
            return fail_exception(frame, esr,
                                  "invalid initial refill HPPIR report");
        }
        pending_hppir_seen = true;
        lab_puts("[gicv2-lab] highest-priority resident LR visible\n");
        lab_kv_hex64("GuestHPPIR_all_pending", argument);
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_REFILL_ACTIVE: {
        struct gicv2_lr_snapshot active;
        uint32_t delivery = active_reports;
        bool valid;

        if (!pending_hppir_seen || delivery >= GICV2_REFILL_DELIVERIES ||
            eoi_reports != delivery || completion_reports != delivery ||
            !irq_state_argument_valid(
                argument, refill_expected_iar(delivery),
                refill_expected_rpr(delivery),
                refill_expected_hppir(delivery))) {
            return fail_exception(frame, esr,
                                  "invalid refill active report");
        }

        valid = gicv2_capture_refill_active(delivery, &active);
        lab_puts("[gicv2-lab] refill delivery active\n");
        dump_refill_state(&active, delivery, 0, argument);
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid refill active state");
        }

        active_reports++;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_REFILL_EOI: {
        struct gicv2_lr_snapshot priority_drop;
        uint32_t delivery = eoi_reports;
        bool valid;

        if (delivery >= GICV2_REFILL_DELIVERIES ||
            active_reports != delivery + 1 ||
            completion_reports != delivery ||
            !irq_state_argument_valid(
                argument, refill_expected_iar(delivery),
                GICV2_GICV_RPR_IDLE_EXPECTED,
                refill_expected_hppir(delivery))) {
            return fail_exception(frame, esr,
                                  "invalid refill EOI report");
        }

        valid = gicv2_capture_refill_drop(delivery, &priority_drop);
        lab_puts("[gicv2-lab] refill delivery priority dropped\n");
        dump_refill_state(&priority_drop, delivery, 1, argument);
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid refill priority-drop state");
        }

        eoi_reports++;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_REFILL_COMPLETE: {
        struct gicv2_lr_transition completion;
        uint32_t delivery = completion_reports;
        bool valid;

        if (delivery >= GICV2_REFILL_DELIVERIES ||
            active_reports != delivery + 1 ||
            eoi_reports != delivery + 1 ||
            !irq_state_argument_valid(
                argument, refill_expected_iar(delivery),
                GICV2_GICV_RPR_IDLE_EXPECTED,
                refill_expected_hppir(delivery))) {
            return fail_exception(frame, esr,
                                  "invalid refill completion report");
        }

        valid = gicv2_complete_refill_delivery(delivery, &completion);
        lab_puts("[gicv2-lab] refill delivery deactivated\n");
        dump_refill_state(&completion.before, delivery, 2, argument);
        if (delivery == 0) {
            lab_puts("[gicv2-lab] freed LR0 refilled from EL2 queue\n");
            lab_kv_hex64("EL2SoftwareQueue_after_refill",
                         gicv2_software_queue_entry());
            DUMP_SNAPSHOT(&completion.after, "refilled");
        } else if (delivery + 1 == GICV2_REFILL_DELIVERIES) {
            lab_puts("[gicv2-lab] exhausted LR set restored\n");
            DUMP_SNAPSHOT(&completion.after, "cleared");
        }
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid refill completion state");
        }

        completion_reports++;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_EXIT:
        if (!guest_report_seen || !stage2_fault_seen || !h2_pass_seen ||
            !irq_ready_seen || !pending_hppir_seen ||
            active_reports != GICV2_REFILL_DELIVERIES ||
            eoi_reports != GICV2_REFILL_DELIVERIES ||
            completion_reports != GICV2_REFILL_DELIVERIES ||
            gicv2_software_queue_entry() != 0 || argument != 0) {
            return fail_exception(frame, esr, "guest exited too early");
        }
        lab_puts("[gicv2-lab] H4g PASS\n");
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
    lab_puts("[gicv2-lab] H4g FAIL: physical IRQ is forbidden\n");
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
