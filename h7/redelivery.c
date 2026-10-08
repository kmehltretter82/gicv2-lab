/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "guest_support.h"

const char guest_test_name[] = "H8c";

void h7_main(void)
{
    guest_init(0x201, UINT32_C(0x00400000), GUEST_SPI42);
    guest_write32(GUEST_GICD + 0x204, GUEST_SPI42);
    uint32_t first = guest_read32(GUEST_GICC + 0xc);
    guest_expect("GuestIAR_first", first, 42);
    guest_expect("GuestRPR_first", guest_read32(GUEST_GICC + 0x14), 0x40);
    guest_expect("GuestActive_first", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestPending_first", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_marker("first active");

    guest_write32(GUEST_GICD + 0x204, GUEST_SPI42);
    guest_expect("GuestPending_repend", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestActive_repend", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestHPPIR_repend", guest_read32(GUEST_GICC + 0x18), 1023);
    guest_marker("active and pending");

    guest_write32(GUEST_GICC + 0x10, first);
    guest_expect("GuestRPR_after_first_eoir", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestActive_after_first_eoir", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestPending_after_first_eoir", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestIAR_before_first_dir", guest_read32(GUEST_GICC + 0xc), 1023);
    guest_marker("first priority dropped");

    guest_write32(GUEST_GICC + 0x1000, first);
    guest_expect("GuestActive_after_first_dir", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, 0);
    guest_expect("GuestPending_after_first_dir", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestHPPIR_after_first_dir", guest_read32(GUEST_GICC + 0x18), 42);
    guest_marker("eligible again");

    uint32_t second = guest_read32(GUEST_GICC + 0xc);
    guest_expect("GuestIAR_second", second, 42);
    guest_expect("GuestRPR_second", guest_read32(GUEST_GICC + 0x14), 0x40);
    guest_expect("GuestActive_second", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestPending_second", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_marker("second active");
    guest_write32(GUEST_GICC + 0x10, second);
    guest_expect("GuestRPR_after_second_eoir", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestActive_after_second_eoir", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_write32(GUEST_GICC + 0x1000, second);
    guest_marker("second deactivated");
    guest_finish();
}
