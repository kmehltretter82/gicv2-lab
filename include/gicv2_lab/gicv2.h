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
bool gicv2_capture_empty(struct gicv2_lr_snapshot *empty);
bool gicv2_arm_hyp_timer(uint64_t *delay_ticks, uint64_t *start_count,
                         uint32_t *control);
bool gicv2_service_hyp_timer(uint32_t *iar, uint32_t *control,
                             uint64_t *elapsed_ticks,
                             struct gicv2_lr_snapshot *pending);
bool gicv2_hyp_timer_fired(void);
bool gicv2_capture_wake_active(struct gicv2_lr_snapshot *active);
bool gicv2_capture_wake_drop(struct gicv2_lr_snapshot *priority_drop);
bool gicv2_finish_wake(struct gicv2_lr_transition *transition);
uint32_t gicv2_acknowledge_physical_irq(void);

#endif
