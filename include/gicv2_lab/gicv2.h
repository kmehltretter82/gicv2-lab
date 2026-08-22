/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_GICV2_H
#define GICV2_LAB_GICV2_H

#include <stdbool.h>
#include <stdint.h>

#define GICV2_SNAPSHOT_LRS 4

struct gicv2_lr_snapshot {
    uint32_t hcr;
    uint32_t vmcr;
    uint32_t misr;
    uint32_t eisr[2];
    uint32_t elrsr[2];
    uint32_t apr;
    uint32_t lr[GICV2_SNAPSHOT_LRS];
};

struct gicv2_lr_transition {
    struct gicv2_lr_snapshot before;
    struct gicv2_lr_snapshot after;
};

bool gicv2_init(uint32_t gich_vtr, struct gicv2_lr_snapshot *initial);
bool gicv2_inject_full_lr_set(struct gicv2_lr_snapshot *pending);
bool gicv2_capture_refill_active(uint32_t delivery,
                                 struct gicv2_lr_snapshot *active);
bool gicv2_capture_refill_drop(uint32_t delivery,
                               struct gicv2_lr_snapshot *priority_drop);
bool gicv2_complete_refill_delivery(
    uint32_t delivery, struct gicv2_lr_transition *transition);
uint32_t gicv2_software_queue_entry(void);
uint32_t gicv2_acknowledge_physical_irq(void);

#endif
