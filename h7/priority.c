/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "guest_support.h"

const char guest_test_name[] = "H8b";

void h7_main(void)
{
    /* 42: priority 0x80; 43: priority 0x20. BPR=2 separates group priorities. */
    guest_init(1, UINT32_C(0x20800000), GUEST_SPIS);
    guest_write32(GUEST_GICC + 4, 0x80);
    guest_write32(GUEST_GICD + 0x204, GUEST_SPI42);
    guest_expect("GuestPMR_masked", guest_read32(GUEST_GICC + 4), 0x80);
    guest_expect("GuestPending_masked", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestIAR_masked", guest_read32(GUEST_GICC + 0xc), 1023);
    guest_expect("GuestRPR_masked", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestActive_masked", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, 0);
    guest_marker("priority masked");

    guest_write32(GUEST_GICC + 4, 0xf8);
    uint32_t low = guest_read32(GUEST_GICC + 0xc);
    guest_expect("GuestIAR_low", low, 42);
    guest_expect("GuestRPR_low", guest_read32(GUEST_GICC + 0x14), 0x80);
    guest_expect("GuestActive_low", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_marker("low active");

    guest_write32(GUEST_GICD + 0x204, GUEST_SPI43);
    guest_expect("GuestPending_high", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI43);
    uint32_t high = guest_read32(GUEST_GICC + 0xc);
    guest_expect("GuestIAR_high", high, 43);
    guest_expect("GuestRPR_both", guest_read32(GUEST_GICC + 0x14), 0x20);
    guest_expect("GuestActive_both", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPIS);
    guest_marker("both active");

    guest_write32(GUEST_GICC + 0x10, high);
    guest_expect("GuestRPR_after_high_eoi", guest_read32(GUEST_GICC + 0x14), 0x80);
    guest_expect("GuestActive_after_high_eoi", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_marker("high completed");
    guest_write32(GUEST_GICC + 0x10, low);
    guest_marker("low completed");
    guest_finish();
}
