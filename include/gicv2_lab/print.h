#ifndef GICV2_LAB_PRINT_H
#define GICV2_LAB_PRINT_H

#include <stdint.h>

void lab_puts(const char *text);
void lab_put_hex64(uint64_t value);
void lab_kv_hex64(const char *name, uint64_t value);
void lab_kv_dec(const char *name, uint32_t value);

#endif
