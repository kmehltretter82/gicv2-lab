/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_HVC_H
#define GICV2_LAB_HVC_H

#define HVC_REPORT     0x100
#define HVC_PASS       0x101
#define HVC_FAIL       0x102
#define HVC_EXIT       0x103
#define HVC_IRQ_READY        0x104
#define HVC_CONTEXT_PAUSE    0x105
#define HVC_HIGH_ACTIVE      0x106
#define HVC_HIGH_EOI         0x107
#define HVC_HIGH_DEACTIVATE  0x108
#define HVC_LOW_RESUMED      0x109
#define HVC_LOW_EOI          0x10a
#define HVC_LOW_DEACTIVATE   0x10b

#define HVC_PASS_MAGIC 0x600d

#endif
