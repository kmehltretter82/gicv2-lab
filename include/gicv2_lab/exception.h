/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_EXCEPTION_H
#define GICV2_LAB_EXCEPTION_H

#include <stdint.h>

struct exception_frame {
    uint64_t x[31];
    uint64_t vector_slot;
};

enum exception_action {
    EXCEPTION_HALT = 0,
    EXCEPTION_RESUME = 1,
};

uint64_t exception_dispatch(struct exception_frame *frame);

#endif
