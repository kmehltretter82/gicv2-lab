/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GICV2_LAB_GUEST_SUPPORT_H
#define GICV2_LAB_GUEST_SUPPORT_H

#include <stdint.h>

#define GUEST_GICD UINT64_C(0x08000000)
#define GUEST_GICC UINT64_C(0x08010000)
#define GUEST_SPI42 (UINT32_C(1) << 10)
#define GUEST_SPI43 (UINT32_C(1) << 11)
#define GUEST_SPIS (GUEST_SPI42 | GUEST_SPI43)

extern const char guest_test_name[];
uint32_t guest_read32(uint64_t address);
void guest_write32(uint64_t address, uint32_t value);
void guest_expect(const char *name, uint64_t value, uint64_t expected);
void guest_marker(const char *checkpoint);
void guest_init(uint32_t control, uint32_t priorities, uint32_t enabled);
void guest_finish(void) __attribute__((noreturn));

#endif
