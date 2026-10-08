/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "guest_support.h"

const char guest_test_name[] = "H8a";

void h7_main(void)
{
    guest_init(0x201, UINT32_C(0x00400000), GUEST_SPI42);
    guest_write32(GUEST_GICD + 0x204, GUEST_SPI42);
    guest_expect("GuestPending_before_iar", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestHPPIR_pending", guest_read32(GUEST_GICC + 0x18), 42);
    guest_marker("pending");
    uint32_t iar = guest_read32(GUEST_GICC + 0xc);
    guest_expect("GuestIAR_active", iar, 42);
    guest_expect("GuestRPR_active", guest_read32(GUEST_GICC + 0x14), 0x40);
    guest_expect("GuestActive_after_iar", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestPending_after_iar", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_marker("active");

    guest_write32(GUEST_GICC + 0x10, iar);
    guest_expect("GuestRPR_after_eoir", guest_read32(GUEST_GICC + 0x14), 0xff);
    guest_expect("GuestActive_after_eoir", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, GUEST_SPI42);
    guest_expect("GuestPending_after_eoir", guest_read32(GUEST_GICD + 0x204) & GUEST_SPIS, 0);
    guest_expect("GuestIAR_before_dir", guest_read32(GUEST_GICC + 0xc), 1023);
    guest_marker("priority dropped");

    guest_write32(GUEST_GICC + 0x1000, iar);
    guest_expect("GuestActive_after_dir", guest_read32(GUEST_GICD + 0x304) & GUEST_SPIS, 0);
    guest_marker("deactivated");
    guest_finish();
}
