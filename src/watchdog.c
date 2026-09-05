/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/platform.h"
#include "gicv2_lab/watchdog.h"

#define PM_RSTC                  UINT64_C(0x1c)
#define PM_WDOG                  UINT64_C(0x24)

#define PM_PASSWORD              UINT32_C(0x5a000000)
#define PM_PASSWORD_MASK         UINT32_C(0xff000000)
#define PM_RSTC_WRCFG_MASK       UINT32_C(0x00000030)
#define PM_RSTC_WRCFG_FULL_RESET UINT32_C(0x00000020)

/*
 * Arm the BCM2711 watchdog for a full reset after `ticks` 65536 Hz ticks.
 *
 * This is the lab's only MMIO outside the UART, timer, and GIC blocks. It
 * exists so that a hardware run returns the board to its vendor kernel
 * without physical intervention: the monitor halts after printing its trace,
 * the watchdog then performs a warm reset, and because the firmware's
 * one-shot `tryboot` selection has already been consumed the board comes back
 * on the normal `config.txt`. It also bounds a hang, since a halted board
 * cannot be recovered over the serial line.
 *
 * The register interface is the one Linux's bcm2835 restart handler uses. The
 * password byte must be exactly 0x5a on every write, so the read-modify-write
 * of PM_RSTC discards the top byte rather than preserving whatever it read.
 */
void watchdog_arm(uint32_t ticks)
{
    uint32_t rstc;

    if (ticks > WATCHDOG_MAX_TICKS) {
        ticks = WATCHDOG_MAX_TICKS;
    }

    mmio_write32(PI400_PM_BASE + PM_WDOG, PM_PASSWORD | ticks);

    rstc = mmio_read32(PI400_PM_BASE + PM_RSTC);
    rstc &= ~PM_PASSWORD_MASK;
    rstc &= ~PM_RSTC_WRCFG_MASK;
    rstc |= PM_RSTC_WRCFG_FULL_RESET;
    mmio_write32(PI400_PM_BASE + PM_RSTC, PM_PASSWORD | rstc);
}
