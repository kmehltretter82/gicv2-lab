/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_WATCHDOG_H
#define GICV2_LAB_WATCHDOG_H

#include <stdint.h>

/*
 * The BCM2711 watchdog counts at a fixed 65536 Hz and its timeout field is
 * 20 bits, so the longest arming interval is just under 16 seconds.
 */
#define WATCHDOG_TICKS_PER_SECOND UINT32_C(65536)
#define WATCHDOG_MAX_TICKS        UINT32_C(0x000fffff)

void watchdog_arm(uint32_t ticks);

#endif
