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

struct gicv2_vcpu_context {
    uint32_t hcr;
    uint32_t vmcr;
    uint32_t apr;
    uint32_t lr[GICV2_SNAPSHOT_LRS];
};

struct gicv2_context_switch {
    uint32_t completed_steps;
    struct gicv2_lr_snapshot low_active;
    struct gicv2_lr_snapshot saved;
    struct gicv2_lr_snapshot disabled;
    struct gicv2_lr_snapshot quiescent;
    struct gicv2_lr_snapshot restored_disabled;
    struct gicv2_lr_snapshot restored;
    struct gicv2_vcpu_context context;
};

bool gicv2_init(uint32_t gich_vtr, struct gicv2_lr_snapshot *initial);
bool gicv2_inject_low(struct gicv2_lr_snapshot *pending);
bool gicv2_pause_save_restore(struct gicv2_context_switch *context_switch);
bool gicv2_capture_both_active(struct gicv2_lr_snapshot *active);
bool gicv2_capture_high_drop(struct gicv2_lr_snapshot *priority_drop);
bool gicv2_capture_high_deactivated(struct gicv2_lr_snapshot *deactivated);
bool gicv2_capture_low_resumed(struct gicv2_lr_snapshot *resumed);
bool gicv2_capture_low_drop(struct gicv2_lr_snapshot *priority_drop);
bool gicv2_finish_context_test(struct gicv2_lr_transition *transition);
uint32_t gicv2_acknowledge_physical_irq(void);

#endif
