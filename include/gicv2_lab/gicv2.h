/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_GICV2_H
#define GICV2_LAB_GICV2_H

#include <stdbool.h>
#include <stdint.h>

struct gicv2_maintenance_trace {
    uint32_t iar;
    uint32_t hcr;
    uint32_t misr;
    uint32_t eisr[2];
    uint32_t elrsr[2];
    uint32_t apr;
    uint32_t lr0;
    uint32_t misr_after;
    uint32_t eisr_after[2];
    uint32_t elrsr_after[2];
    uint32_t lr0_after;
};

bool gicv2_init(uint32_t lr_count);
bool gicv2_inject_test_irq(uint32_t *lr_value);
void gicv2_acknowledge_maintenance(struct gicv2_maintenance_trace *trace);
bool gicv2_maintenance_trace_valid(
    const struct gicv2_maintenance_trace *trace);

#endif
