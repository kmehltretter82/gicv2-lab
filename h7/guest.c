/* SPDX-License-Identifier: GPL-2.0-or-later */
/* H7's complete architectural contract is in docs/H7_KVM.md. */
#include <stdint.h>
#include "gicv2_lab/print.h"
#include "gicv2_lab/uart.h"

#define GICD UINT64_C(0x08000000)
#define GICC UINT64_C(0x08010000)
#define UART UINT64_C(0x09000000)
#define SPI_BIT (UINT32_C(1) << (42 - 32))

static uint32_t read32(uint64_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

void uart_putc(char value)
{
    while (read32(UART + 0x18) & (1U << 5)) {
        __asm__ volatile("yield");
    }
    write32(UART, (uint8_t)value);
}

static __attribute__((noreturn)) void halt(void)
{
    for (;;) {
        __asm__ volatile("wfe");
    }
}

static void expect(const char *key, uint64_t value, uint64_t expected)
{
    lab_kv_hex64(key, value);
    if (value != expected) {
        lab_puts("[gicv2-lab] H7 FAIL: ");
        lab_puts(key);
        lab_puts("\n");
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
    lab_puts("[gicv2-lab] H7 FAIL: unexpected exception\n");
    halt();
}

void h7_main(void)
{
    uint64_t current_el;
    lab_puts("[gicv2-lab] H7 SPI lifecycle\n");
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    expect("GuestCurrentEL", current_el >> 2, 1);
    __asm__ volatile("msr cntv_ctl_el0, xzr; msr cntp_ctl_el0, xzr; isb" ::: "memory");

    /* One CPU, no Security Extensions in the guest's GIC model, >=64 INTIDs.
     * IRQs stay masked: IAR is polled and no interrupt handler races the test.
     */
    uint32_t typer = read32(GICD + 4);
    lab_kv_hex64("GICD_TYPER", typer); /* capability, not a compared field */
    if ((typer & 0x1f) < 1 || (typer & ((7U << 5) | (1U << 10)))) {
        lab_puts("[gicv2-lab] H7 FAIL: GIC capability precondition\n");
        halt();
    }
    write32(GICC, 0);
    write32(GICD, 0);
    write32(GICD + 0x180, UINT32_MAX);
    write32(GICD + 0x184, UINT32_MAX);
    write32(GICD + 0x280, UINT32_MAX);
    write32(GICD + 0x284, UINT32_MAX);
    write32(GICD + 0x80, 0);
    write32(GICD + 0x84, 0);
    write32(GICD + 0x428, UINT32_C(0x00400000)); /* priority 0x40 for SPI 42 */
    write32(GICD + 0x828, UINT32_C(0x00010000)); /* target CPU 0 */
    write32(GICD + 0xc08, 0);                  /* SPIs 32-47 level-triggered */
    write32(GICD + 0x104, SPI_BIT);
    write32(GICC + 4, 0xf8);
    write32(GICC + 8, 2);
    write32(GICD, 1);
    write32(GICC, 1);                         /* combined priority drop/EOI */
    expect("GuestGICC_CTLR", read32(GICC), 1);
    expect("GuestGICC_PMR", read32(GICC + 4), 0xf8);
    expect("GuestGICC_BPR", read32(GICC + 8), 2);
    expect("GuestIAR_initial", read32(GICC + 0xc), 1023);
    expect("GuestRPR_initial", read32(GICC + 0x14), 0xff);
    lab_puts("[gicv2-lab] H7 interface ready\n");

    /* A software pending bit avoids any external interrupt-line timing. */
    write32(GICD + 0x204, SPI_BIT);
    expect("GuestPending_before_iar", read32(GICD + 0x204) & SPI_BIT, SPI_BIT);
    expect("GuestHPPIR_pending", read32(GICC + 0x18), 42);
    lab_puts("[gicv2-lab] H7 SPI pending\n");
    uint32_t iar = read32(GICC + 0xc);
    expect("GuestIAR_active", iar, 42);
    expect("GuestRPR_active", read32(GICC + 0x14), 0x40);
    expect("GuestActive_after_iar", read32(GICD + 0x304) & SPI_BIT, SPI_BIT);
    lab_puts("[gicv2-lab] H7 SPI active\n");
    write32(GICC + 0x10, iar);
    expect("GuestRPR_after_eoi", read32(GICC + 0x14), 0xff);
    expect("GuestActive_after_eoi", read32(GICD + 0x304) & SPI_BIT, 0);
    expect("GuestPending_after_eoi", read32(GICD + 0x204) & SPI_BIT, 0);
    expect("GuestHPPIR_final", read32(GICC + 0x18), 1023);
    expect("GuestIAR_final", read32(GICC + 0xc), 1023);
    lab_puts("[gicv2-lab] H7 SPI completed\n");
    lab_puts("[gicv2-lab] H7 PASS\n");
    halt();
}
