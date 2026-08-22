/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_GICV2_H
#define GICV2_LAB_GICV2_H

#include <stdbool.h>
#include <stdint.h>

struct gicv2_lr_snapshot {
    uint32_t hcr;
    uint32_t vmcr;
    uint32_t misr;
    uint32_t eisr[2];
    uint32_t elrsr[2];
    uint32_t apr;
    uint32_t lr0;
};

struct gicv2_maintenance_trace {
    uint32_t iar;
    struct gicv2_lr_snapshot before;
    struct gicv2_lr_snapshot after;
};

bool gicv2_init(uint32_t lr_count, struct gicv2_lr_snapshot *initial);
bool gicv2_inject_test_irq(struct gicv2_lr_snapshot *pending);
void gicv2_capture_lr0(struct gicv2_lr_snapshot *snapshot);
bool gicv2_active_snapshot_valid(const struct gicv2_lr_snapshot *snapshot);
void gicv2_acknowledge_maintenance(struct gicv2_maintenance_trace *trace);
bool gicv2_maintenance_trace_valid(
    const struct gicv2_maintenance_trace *trace);

#endif
