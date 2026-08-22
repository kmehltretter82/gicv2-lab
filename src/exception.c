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
#define ESR_EC_WFX_TRAP   UINT64_C(0x01)
#define ESR_WFX_TI_MASK   UINT64_C(0x3)
#define ESR_WFX_TI_WFI    UINT64_C(0)

#define VECTOR_CURRENT_SPX_SYNC UINT64_C(4)
#define VECTOR_CURRENT_SPX_IRQ  UINT64_C(5)
#define VECTOR_LOWER_A64_SYNC   UINT64_C(8)
#define VECTOR_LOWER_A64_IRQ    UINT64_C(9)
#define GUEST_CURRENTEL_VALUE   UINT64_C(4)
#define S2_LEVEL2_TRANSLATION_FAULT UINT64_C(6)
#define SPSR_MODE_MASK UINT64_C(0xf)
#define SPSR_EL1H      UINT64_C(5)
#define SPSR_IRQ_MASK  (UINT64_C(1) << 7)

#define HCR_EL2_BASE UINT64_C(0x80000011)
#define HCR_EL2_TWI  (UINT64_C(1) << 13)

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
static bool wfi_ready_seen;
static bool wfi_trap_seen;
static bool timer_irq_seen;
static bool wake_active_seen;
static bool wake_drop_seen;
static bool wake_deactivated_seen;
static bool wfi_resumed_seen;
static bool el2_brk_seen;

extern char guest_wfi_instruction[];
extern char guest_after_wfi_instruction[];

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
    lab_puts("[gicv2-lab] H4h FAIL: ");
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
        struct gicv2_lr_snapshot empty;

        if (!h2_pass_seen || irq_ready_seen ||
            !irq_ready_argument_valid(argument) ||
            read_hcr_el2() != HCR_EL2_BASE ||
            !gicv2_capture_empty(&empty)) {
            return fail_exception(frame, esr, "invalid IRQ-ready report");
        }
        irq_ready_seen = true;
        lab_puts("[gicv2-lab] EL1 virtual interface ready\n");
        lab_kv_hex64("GuestGICV_CTLR", (uint32_t)argument);
        lab_kv_hex64("GuestGICV_PMR",
                     (argument >> IRQ_READY_PMR_SHIFT) & 0xff);
        lab_kv_hex64("GuestGICV_BPR",
                     (argument >> IRQ_READY_BPR_SHIFT) & 7);
        lab_puts("[gicv2-lab] virtual interface empty before WFI\n");
        DUMP_SNAPSHOT(&empty, "before_wfi");
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_WFI_READY: {
        struct gicv2_lr_snapshot empty;
        uint64_t hcr_before = read_hcr_el2();

        lab_kv_hex64("GuestHPPIR_before_wfi", argument);
        if (!irq_ready_seen || wfi_ready_seen || wfi_trap_seen ||
            timer_irq_seen || argument !=
                GICV2_GICV_HPPIR_SPURIOUS_EXPECTED ||
            hcr_before != HCR_EL2_BASE ||
            !gicv2_capture_empty(&empty)) {
            return fail_exception(frame, esr,
                                  "invalid WFI-ready report");
        }

        write_hcr_el2(HCR_EL2_BASE | HCR_EL2_TWI);
        if (read_hcr_el2() != (HCR_EL2_BASE | HCR_EL2_TWI)) {
            return fail_exception(frame, esr, "failed to arm WFI trap");
        }

        wfi_ready_seen = true;
        lab_puts("[gicv2-lab] guest WFI trap armed\n");
        lab_kv_hex64("HCR_EL2_before_wfi", hcr_before);
        lab_kv_hex64("HCR_EL2_wfi_trap_armed", read_hcr_el2());
        DUMP_SNAPSHOT(&empty, "wfi_trap_armed");
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_WAKE_ACTIVE: {
        struct gicv2_lr_snapshot active;
        bool valid;

        if (!wfi_trap_seen || !timer_irq_seen || wake_active_seen ||
            wake_drop_seen || wake_deactivated_seen || wfi_resumed_seen ||
            !irq_state_argument_valid(
                argument, GICV2_WAKE_INTID, GICV2_WAKE_PRIORITY,
                GICV2_GICV_HPPIR_SPURIOUS_EXPECTED)) {
            return fail_exception(frame, esr, "invalid wake active report");
        }

        valid = gicv2_capture_wake_active(&active);
        lab_puts("[gicv2-lab] timer-injected virtual IRQ active\n");
        dump_irq_state("GuestIAR_wake_active", "GuestRPR_wake_active",
                       "GuestHPPIR_wake_active", argument);
        lab_kv_hex64("GuestIRQState_wake_active", argument);
        DUMP_SNAPSHOT(&active, "wake_active");
        if (!valid) {
            return fail_exception(frame, esr, "invalid wake active state");
        }

        wake_active_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_WAKE_EOI: {
        struct gicv2_lr_snapshot priority_drop;
        bool valid;

        if (!wake_active_seen || wake_drop_seen || wake_deactivated_seen ||
            wfi_resumed_seen ||
            !irq_state_argument_valid(
                argument, GICV2_WAKE_INTID,
                GICV2_GICV_RPR_IDLE_EXPECTED,
                GICV2_GICV_HPPIR_SPURIOUS_EXPECTED)) {
            return fail_exception(frame, esr, "invalid wake EOI report");
        }

        valid = gicv2_capture_wake_drop(&priority_drop);
        lab_puts("[gicv2-lab] wake IRQ priority dropped\n");
        dump_irq_state("GuestIAR_wake_drop", "GuestRPR_wake_drop",
                       "GuestHPPIR_wake_drop", argument);
        lab_kv_hex64("GuestIRQState_wake_drop", argument);
        DUMP_SNAPSHOT(&priority_drop, "wake_drop");
        if (!valid) {
            return fail_exception(frame, esr, "invalid wake drop state");
        }

        wake_drop_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_WAKE_DEACTIVATE: {
        struct gicv2_lr_transition completion;
        bool valid;

        if (!wake_drop_seen || wake_deactivated_seen || wfi_resumed_seen ||
            !irq_state_argument_valid(
                argument, GICV2_WAKE_INTID,
                GICV2_GICV_RPR_IDLE_EXPECTED,
                GICV2_GICV_HPPIR_SPURIOUS_EXPECTED)) {
            return fail_exception(frame, esr,
                                  "invalid wake deactivation report");
        }

        valid = gicv2_finish_wake(&completion);
        lab_puts("[gicv2-lab] wake IRQ deactivated\n");
        dump_irq_state("GuestIAR_wake_deactivated",
                       "GuestRPR_wake_deactivated",
                       "GuestHPPIR_wake_deactivated", argument);
        lab_kv_hex64("GuestIRQState_wake_deactivated", argument);
        DUMP_SNAPSHOT(&completion.before, "wake_deactivated");
        lab_puts("[gicv2-lab] wake LR cleared\n");
        DUMP_SNAPSHOT(&completion.after, "cleared");
        if (!valid) {
            return fail_exception(frame, esr,
                                  "invalid wake deactivation state");
        }

        wake_deactivated_seen = true;
        frame->x[0] = 0;
        return EXCEPTION_RESUME;
    }

    case HVC_WFI_RESUMED:
        if (!wake_deactivated_seen || wfi_resumed_seen || argument != 1 ||
            !gicv2_hyp_timer_fired() || read_hcr_el2() != HCR_EL2_BASE) {
            return fail_exception(frame, esr,
                                  "invalid post-WFI resume report");
        }
        wfi_resumed_seen = true;
        lab_puts("[gicv2-lab] guest resumed after timer-woken WFI\n");
        lab_kv_dec("GuestIRQCount_after_wfi", (uint32_t)argument);
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_EXIT:
        if (!guest_report_seen || !stage2_fault_seen || !h2_pass_seen ||
            !irq_ready_seen || !wfi_ready_seen || !wfi_trap_seen ||
            !timer_irq_seen || !wake_active_seen || !wake_drop_seen ||
            !wake_deactivated_seen || !wfi_resumed_seen ||
            !gicv2_hyp_timer_fired() || read_hcr_el2() != HCR_EL2_BASE ||
            argument != 0) {
            return fail_exception(frame, esr, "guest exited too early");
        }
        lab_puts("[gicv2-lab] H4h PASS\n");
        return EXCEPTION_HALT;

    default:
        return fail_exception(frame, esr, "unknown HVC operation");
    }
}

static uint64_t handle_physical_irq(struct exception_frame *frame)
{
    struct gicv2_lr_snapshot pending = { 0 };
    uint64_t elapsed_ticks = 0;
    uint32_t control = 0;
    uint32_t iar = 1023;
    bool valid;

    valid = gicv2_service_hyp_timer(&iar, &control, &elapsed_ticks,
                                    &pending);
    lab_puts("[gicv2-lab] physical IRQ observed\n");
    lab_kv_dec("physical_irq_vector_slot", (uint32_t)frame->vector_slot);
    lab_kv_dec("PhysicalIAR_hyp_timer", iar & UINT32_C(0x3ff));
    lab_kv_hex64("CNTHP_CTL_expired", control);
    lab_kv_hex64("CNTHP_elapsed_ticks", elapsed_ticks);
    lab_kv_hex64("HCR_EL2_timer_irq", read_hcr_el2());
    lab_kv_hex64("ELR_EL2_timer_irq", read_elr_el2());
    lab_kv_hex64("SPSR_EL2_timer_irq", read_spsr_el2());

    if (!wfi_trap_seen || timer_irq_seen ||
        frame->vector_slot != VECTOR_LOWER_A64_IRQ ||
        read_hcr_el2() != HCR_EL2_BASE ||
        read_elr_el2() !=
            (uint64_t)(uintptr_t)guest_after_wfi_instruction ||
        (read_spsr_el2() & SPSR_MODE_MASK) != SPSR_EL1H ||
        (read_spsr_el2() & SPSR_IRQ_MASK) != 0 || !valid) {
        lab_puts("[gicv2-lab] H4h FAIL: unexpected physical IRQ\n");
        return EXCEPTION_HALT;
    }

    timer_irq_seen = true;
    lab_puts("[gicv2-lab] expected hypervisor timer IRQ\n");
    DUMP_SNAPSHOT(&pending, "timer_injected_pending");
    return EXCEPTION_RESUME;
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

    if (frame->vector_slot == VECTOR_LOWER_A64_SYNC &&
        ec == ESR_EC_WFX_TRAP) {
        uint64_t delay_ticks;
        uint64_t start_count;
        uint64_t spsr = read_spsr_el2();
        uint32_t control;

        if (!wfi_ready_seen || wfi_trap_seen || timer_irq_seen ||
            (iss & ESR_WFX_TI_MASK) != ESR_WFX_TI_WFI ||
            elr != (uint64_t)(uintptr_t)guest_wfi_instruction ||
            read_hcr_el2() != (HCR_EL2_BASE | HCR_EL2_TWI) ||
            (spsr & SPSR_MODE_MASK) != SPSR_EL1H ||
            (spsr & SPSR_IRQ_MASK) != 0) {
            return fail_exception(frame, esr, "invalid trapped WFI");
        }

        lab_puts("[gicv2-lab] expected trapped guest WFI\n");
        dump_exception(frame, esr);
        lab_kv_hex64("HCR_EL2_wfi_trap", read_hcr_el2());

        write_hcr_el2(HCR_EL2_BASE);
        if (read_hcr_el2() != HCR_EL2_BASE ||
            !gicv2_arm_hyp_timer(&delay_ticks, &start_count, &control)) {
            return fail_exception(frame, esr,
                                  "failed to arm WFI wake timer");
        }

        wfi_trap_seen = true;
        lab_kv_hex64("HCR_EL2_wfi_reexecute", read_hcr_el2());
        lab_kv_hex64("CNTHP_delay_ticks", delay_ticks);
        lab_kv_hex64("CNTHP_start_count", start_count);
        lab_kv_hex64("CNTHP_CTL_armed", control);
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
