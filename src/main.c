/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/layout.h"
#include "gicv2_lab/platform.h"
#include "gicv2_lab/print.h"
#include "gicv2_lab/stage2.h"
#include "gicv2_lab/uart.h"

extern void enter_guest(uint64_t entry, uint64_t stack)
    __attribute__((noreturn));
extern char guest_start[];

static void halt_forever(void)
{
    for (;;) {
        cpu_relax();
    }
}

void lab_main(void)
{
    uint64_t current_el;
    uint32_t gich_vtr;

    uart_init();
    lab_puts("\n[gicv2-lab] H2 EL2 monitor\n");

    current_el = read_currentel() >> 2;
    lab_kv_dec("CurrentEL", (uint32_t)current_el);
    lab_kv_hex64("MIDR_EL1", read_midr_el1());
    lab_kv_hex64("MPIDR_EL1", read_mpidr_el1());
    lab_kv_hex64("CNTFRQ_EL0", read_cntfrq_el0());
    lab_kv_hex64("ID_AA64PFR0_EL1", read_id_aa64pfr0_el1());
    lab_kv_hex64("HCR_EL2", read_hcr_el2());
    lab_kv_hex64("VTCR_EL2", read_vtcr_el2());
    lab_kv_hex64("VTTBR_EL2", read_vttbr_el2());

    gich_vtr = mmio_read32(PI400_GICH_BASE + GICH_VTR);
    lab_kv_hex64("GICH_VTR", gich_vtr);
    lab_kv_dec("GICH_LRS", (gich_vtr & 0x3f) + 1);

    if (current_el != 2) {
        lab_puts("[gicv2-lab] H2 FAIL: expected EL2\n");
        halt_forever();
    }

    lab_puts("[gicv2-lab] deliberate BRK\n");
    __asm__ volatile("brk #0x471");
    lab_puts("[gicv2-lab] H1 PASS\n");

    stage2_enable();
    lab_kv_hex64("stage2_root", stage2_root_address());
    lab_kv_hex64("stage2_guest_desc", stage2_guest_descriptor());
    lab_kv_hex64("VTCR_EL2_enabled", read_vtcr_el2());
    lab_kv_hex64("VTTBR_EL2_enabled", read_vttbr_el2());
    lab_kv_hex64("HCR_EL2_enabled", read_hcr_el2());

    lab_puts("[gicv2-lab] entering EL1 guest\n");
    enter_guest((uint64_t)(uintptr_t)guest_start, GUEST_STACK_TOP);
}
