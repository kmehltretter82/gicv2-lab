/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Shared fixed platform setup for the contracts in docs/H8_KVM_CONTRACTS.md. */
#include "guest_support.h"
#include "gicv2_lab/print.h"
#include "gicv2_lab/uart.h"

#define UART UINT64_C(0x09000000)

uint32_t guest_read32(uint64_t address)
{
    uint32_t value = *(volatile uint32_t *)(uintptr_t)address;
    __asm__ volatile("dsb sy" ::: "memory");
    return value;
}

void guest_write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

void uart_putc(char value)
{
    while (guest_read32(UART + 0x18) & (1U << 5)) {
        __asm__ volatile("yield");
    }
    guest_write32(UART, (uint8_t)value);
}

static __attribute__((noreturn)) void halt(void)
{
    for (;;) {
        __asm__ volatile("wfe");
    }
}

void guest_marker(const char *checkpoint)
{
    lab_puts("[gicv2-lab] ");
    lab_puts(guest_test_name);
    lab_puts(" ");
    lab_puts(checkpoint);
    lab_puts("\n");
}

void guest_expect(const char *name, uint64_t value, uint64_t expected)
{
    lab_kv_hex64(name, value);
    if (value != expected) {
        guest_marker("FAIL: contract violation");
        halt();
    }
}

void h7_exception(void)
{
    uint64_t esr, elr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    lab_kv_hex64("ESR_EL1", esr);
    lab_kv_hex64("ELR_EL1", elr);
    guest_marker("FAIL: unexpected exception");
    halt();
}

void guest_init(uint32_t control, uint32_t priorities, uint32_t enabled)
{
    uint64_t el, daif, sctlr;
    guest_marker("begin");
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    guest_expect("GuestCurrentEL", el >> 2, 1);
    guest_expect("GuestDAIF", daif, 0x3c0);
    guest_expect("GuestSCTLR_MCI", sctlr & ((1U << 12) | (1U << 2) | 1U), 0);
    __asm__ volatile("msr cntv_ctl_el0, xzr; msr cntp_ctl_el0, xzr; isb" ::: "memory");

    uint32_t typer = guest_read32(GUEST_GICD + 4);
    lab_kv_hex64("GICD_TYPER", typer);
    if ((typer & 0x1f) < 1 || (typer & ((7U << 5) | (1U << 10)))) {
        guest_marker("FAIL: GIC capability precondition");
        halt();
    }
    guest_write32(GUEST_GICC, 0);
    guest_write32(GUEST_GICD, 0);
    guest_write32(GUEST_GICD + 0x180, UINT32_MAX);
    guest_write32(GUEST_GICD + 0x184, UINT32_MAX);
    guest_write32(GUEST_GICD + 0x280, UINT32_MAX);
    guest_write32(GUEST_GICD + 0x284, UINT32_MAX);
    guest_write32(GUEST_GICD + 0x80, 0);
    guest_write32(GUEST_GICD + 0x84, 0);
    guest_write32(GUEST_GICD + 0x428, priorities);
    guest_write32(GUEST_GICD + 0x828, UINT32_C(0x01010000)); /* CPU 0 for 42/43 */
    guest_write32(GUEST_GICD + 0xc08, 0); /* Level; no external line is asserted. */
    guest_write32(GUEST_GICD + 0x104, enabled);
    guest_write32(GUEST_GICC + 4, 0xf8);
    guest_write32(GUEST_GICC + 8, 2);
    guest_write32(GUEST_GICD, 1);
    guest_write32(GUEST_GICC, control);
    guest_expect("GuestGICC_CTLR", guest_read32(GUEST_GICC), control);
    guest_expect("GuestGICC_PMR", guest_read32(GUEST_GICC + 4), 0xf8);
    guest_expect("GuestGICC_BPR", guest_read32(GUEST_GICC + 8), 2);
    guest_expect("GuestIAR_initial", guest_read32(GUEST_GICC + 0xc), 1023);
    guest_expect("GuestRPR_initial", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestPending_initial", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_expect("GuestActive_initial", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, 0);
    guest_marker("interface ready");
}

void guest_finish(void)
{
    guest_expect("GuestRPR_final", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestActive_final", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, 0);
    guest_expect("GuestPending_final", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_expect("GuestHPPIR_final", guest_read32(GUEST_GICC + 0x18), 1023);
    guest_expect("GuestIAR_final", guest_read32(GUEST_GICC + 0xc), 1023);
    guest_marker("PASS");
    halt();
}
