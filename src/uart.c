/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/platform.h"
#include "gicv2_lab/uart.h"

#define PL011_DR      UINT64_C(0x000)
#define PL011_FR      UINT64_C(0x018)
#define PL011_IBRD    UINT64_C(0x024)
#define PL011_FBRD    UINT64_C(0x028)
#define PL011_LCR_H   UINT64_C(0x02c)
#define PL011_CR      UINT64_C(0x030)
#define PL011_ICR     UINT64_C(0x044)

#define PL011_FR_TXFF (UINT32_C(1) << 5)
#define PL011_CR_UARTEN UINT32_C(1)
#define PL011_CR_TXE    (UINT32_C(1) << 8)
#define PL011_CR_RXE    (UINT32_C(1) << 9)
#define PL011_LCR_H_FEN (UINT32_C(1) << 4)
#define PL011_LCR_H_WLEN_8 (UINT32_C(3) << 5)

void uart_init(void)
{
    /*
     * The BCM2711/QEMU platform configures the UART clock to 3 MHz. The
     * 1 + 40/64 divider yields 115200 baud. QEMU's serial backend does not
     * depend on the programmed baud rate, but retaining it matters for a
     * later recoverable hardware run.
     */
    mmio_write32(PI400_PL011_BASE + PL011_CR, 0);
    mmio_write32(PI400_PL011_BASE + PL011_ICR, UINT32_C(0x7ff));
    mmio_write32(PI400_PL011_BASE + PL011_IBRD, 1);
    mmio_write32(PI400_PL011_BASE + PL011_FBRD, 40);
    mmio_write32(PI400_PL011_BASE + PL011_LCR_H,
                 PL011_LCR_H_FEN | PL011_LCR_H_WLEN_8);
    mmio_write32(PI400_PL011_BASE + PL011_CR,
                 PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

void uart_putc(char character)
{
    if (character == '\n') {
        uart_putc('\r');
    }

    while (mmio_read32(PI400_PL011_BASE + PL011_FR) & PL011_FR_TXFF) {
    }

    mmio_write32(PI400_PL011_BASE + PL011_DR, (uint32_t)character);
}
