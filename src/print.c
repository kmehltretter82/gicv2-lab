/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/print.h"
#include "gicv2_lab/uart.h"

static const char digits[] = "0123456789abcdef";

void lab_puts(const char *text)
{
    while (*text != '\0') {
        uart_putc(*text++);
    }
}

void lab_put_hex64(uint64_t value)
{
    int shift;

    lab_puts("0x");
    for (shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0xf]);
    }
}

void lab_kv_hex64(const char *name, uint64_t value)
{
    lab_puts(name);
    lab_puts("=");
    lab_put_hex64(value);
    lab_puts("\n");
}

void lab_kv_dec(const char *name, uint32_t value)
{
    char buffer[10];
    uint32_t count = 0;

    lab_puts(name);
    lab_puts("=");

    if (value == 0) {
        uart_putc('0');
    } else {
        while (value != 0) {
            buffer[count++] = digits[value % 10];
            value /= 10;
        }
        while (count != 0) {
            uart_putc(buffer[--count]);
        }
    }

    lab_puts("\n");
}
