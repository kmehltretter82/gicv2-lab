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

#define ESR_EC_HVC64         UINT64_C(0x16)
#define ESR_EC_DABT_LOWER    UINT64_C(0x24)
#define ESR_EC_BRK64         UINT64_C(0x3c)

#define VECTOR_CURRENT_SPX_SYNC UINT64_C(4)
#define VECTOR_CURRENT_SPX_IRQ  UINT64_C(5)
#define VECTOR_LOWER_A64_SYNC   UINT64_C(8)
#define VECTOR_LOWER_A64_IRQ    UINT64_C(9)
#define GUEST_CURRENTEL_VALUE   UINT64_C(4)
#define S2_LEVEL2_TRANSLATION_FAULT UINT64_C(6)

static bool guest_report_seen;
static bool stage2_fault_seen;
static bool h2_pass_seen;
static bool irq_ready_seen;
static bool guest_irq_eoi_seen;
static bool maintenance_seen;
static bool el2_brk_seen;

extern volatile uint32_t guest_maintenance_seen;

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
    lab_puts("[gicv2-lab] H3 FAIL: ");
    lab_puts(reason);
    lab_puts("\n");
    dump_exception(frame, esr);
    return EXCEPTION_HALT;
}

static uint64_t handle_hvc(struct exception_frame *frame, uint64_t esr)
{
    uint64_t operation = frame->x[0];
    uint64_t argument = frame->x[1];
    uint32_t lr_value;

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

    case HVC_IRQ_READY:
        if (!h2_pass_seen || irq_ready_seen ||
            (uint32_t)argument != H3_GICV_CTLR_EXPECTED ||
            (uint32_t)(argument >> 32) != H3_GICV_PMR_EXPECTED ||
            !gicv2_inject_test_irq(&lr_value)) {
            return fail_exception(frame, esr, "invalid IRQ-ready report");
        }
        irq_ready_seen = true;
        lab_puts("[gicv2-lab] EL1 virtual interface ready\n");
        lab_kv_hex64("GuestGICV_CTLR", (uint32_t)argument);
        lab_kv_hex64("GuestGICV_PMR", (uint32_t)(argument >> 32));
        lab_kv_hex64("GICH_LR0_injected", lr_value);
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_IRQ_EOI:
        if (!irq_ready_seen || guest_irq_eoi_seen ||
            argument != H3_VIRTUAL_INTID) {
            return fail_exception(frame, esr, "invalid virtual IRQ EOI");
        }
        guest_irq_eoi_seen = true;
        lab_puts("[gicv2-lab] EL1 acknowledged and EOIed virtual IRQ\n");
        lab_kv_dec("GuestIAR", (uint32_t)argument);
        frame->x[0] = 0;
        return EXCEPTION_RESUME;

    case HVC_EXIT:
        if (!guest_report_seen || !stage2_fault_seen || !h2_pass_seen ||
            !irq_ready_seen || !guest_irq_eoi_seen || !maintenance_seen ||
            argument != 0) {
            return fail_exception(frame, esr, "guest exited too early");
        }
        lab_puts("[gicv2-lab] H3 PASS\n");
        return EXCEPTION_HALT;

    default:
        return fail_exception(frame, esr, "unknown HVC operation");
    }
}

static void dump_maintenance(
    const struct exception_frame *frame,
    const struct gicv2_maintenance_trace *trace)
{
    lab_kv_dec("maintenance_vector_slot", (uint32_t)frame->vector_slot);
    lab_kv_dec("PhysicalIAR", trace->iar & UINT32_C(0x3ff));
    lab_kv_hex64("GICH_HCR_maintenance", trace->hcr);
    lab_kv_hex64("GICH_MISR", trace->misr);
    lab_kv_hex64("GICH_EISR0", trace->eisr[0]);
    lab_kv_hex64("GICH_EISR1", trace->eisr[1]);
    lab_kv_hex64("GICH_ELRSR0", trace->elrsr[0]);
    lab_kv_hex64("GICH_ELRSR1", trace->elrsr[1]);
    lab_kv_hex64("GICH_APR", trace->apr);
    lab_kv_hex64("GICH_LR0_post_eoi", trace->lr0);
    lab_kv_hex64("GICH_MISR_cleared", trace->misr_after);
    lab_kv_hex64("GICH_EISR0_cleared", trace->eisr_after[0]);
    lab_kv_hex64("GICH_EISR1_cleared", trace->eisr_after[1]);
    lab_kv_hex64("GICH_ELRSR0_cleared", trace->elrsr_after[0]);
    lab_kv_hex64("GICH_ELRSR1_cleared", trace->elrsr_after[1]);
    lab_kv_hex64("GICH_LR0_cleared", trace->lr0_after);
}

static uint64_t handle_physical_irq(struct exception_frame *frame)
{
    struct gicv2_maintenance_trace trace;

    gicv2_acknowledge_maintenance(&trace);
    lab_puts("[gicv2-lab] GIC maintenance interrupt\n");
    dump_maintenance(frame, &trace);

    if (!irq_ready_seen || maintenance_seen ||
        !gicv2_maintenance_trace_valid(&trace)) {
        lab_puts("[gicv2-lab] H3 FAIL: invalid maintenance interrupt\n");
        return EXCEPTION_HALT;
    }

    maintenance_seen = true;
    guest_maintenance_seen = 1;
    __asm__ volatile("dsb sy" : : : "memory");
    lab_puts("[gicv2-lab] maintenance PPI 25 acknowledged\n");
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
