/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_STAGE2_H
#define GICV2_LAB_STAGE2_H

#include <stdint.h>

void stage2_enable(void);
uint64_t stage2_root_address(void);
uint64_t stage2_guest_descriptor(void);

#endif
