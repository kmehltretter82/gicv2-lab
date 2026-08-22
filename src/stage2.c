/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdint.h>

#include "gicv2_lab/gicv2_defs.h"
#include "gicv2_lab/layout.h"
#include "gicv2_lab/stage2.h"

#define S2_TABLE_ENTRIES 512
#define S2_ADDRESS_MASK  UINT64_C(0x0000fffffffff000)

#define S2_DESC_VALID    (UINT64_C(1) << 0)
#define S2_DESC_TABLE    (UINT64_C(1) << 1)
#define S2_MEMATTR_NORMAL (UINT64_C(0xf) << 2)
#define S2_MEMATTR_DEVICE_NGNRE (UINT64_C(1) << 2)
#define S2_S2AP_READ     (UINT64_C(1) << 6)
#define S2_S2AP_WRITE    (UINT64_C(1) << 7)
#define S2_SH_INNER      (UINT64_C(3) << 8)
#define S2_ACCESS_FLAG   (UINT64_C(1) << 10)
#define S2_EXECUTE_NEVER (UINT64_C(1) << 54)

#define VTCR_T0SZ_32BIT_IPA UINT64_C(32)
#define VTCR_SL0_LEVEL1     (UINT64_C(1) << 6)
#define VTCR_IRGN0_WBWA     (UINT64_C(1) << 8)
#define VTCR_ORGN0_WBWA     (UINT64_C(1) << 10)
#define VTCR_SH0_INNER      (UINT64_C(3) << 12)
#define VTCR_RES1           (UINT64_C(1) << 31)

#define HCR_VM              (UINT64_C(1) << 0)
#define HCR_IMO             (UINT64_C(1) << 4)
#define HCR_RW              (UINT64_C(1) << 31)

static uint64_t s2_l1[S2_TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t s2_guest_l2[S2_TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t s2_mmio_l2[S2_TABLE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t s2_gicv_l3[S2_TABLE_ENTRIES] __attribute__((aligned(4096)));

_Static_assert(GUEST_REGION_SIZE == (UINT64_C(1) << 21),
               "H2 uses one 2 MiB stage-2 block");
_Static_assert((GUEST_IPA_BASE & (GUEST_REGION_SIZE - 1)) == 0,
               "guest IPA must be 2 MiB aligned");
_Static_assert(GUEST_STACK_TOP >= GUEST_IPA_BASE &&
               GUEST_STACK_TOP < GUEST_IPA_BASE + GUEST_REGION_SIZE,
               "guest stack must be inside the mapped block");
_Static_assert(MONITOR_LOAD_BASE < GUEST_IPA_BASE,
               "fault target must be outside the guest block");
_Static_assert((PI400_GICV_BASE & UINT64_C(0x1fff)) == 0,
               "GICV base must be 8 KiB aligned");
_Static_assert(GICV_DIR == UINT64_C(0x1000),
               "split EOI requires DIR in the second GICV page");
_Static_assert((uint64_t)PI400_GICV_BASE + GICV_DIR ==
                   UINT64_C(0xff847000),
               "unexpected Pi 400 GICV DIR address");
_Static_assert((((uint64_t)PI400_GICV_BASE >> 12) & UINT64_C(0x1ff)) < 511,
               "GICV mapping must have room for the DIR page");

static void zero_table(uint64_t *table)
{
    size_t index;

    for (index = 0; index < S2_TABLE_ENTRIES; index++) {
        table[index] = 0;
    }
}

void stage2_enable(void)
{
    uint64_t guest_attributes;
    uint64_t gicv_attributes;
    uint64_t root_address = (uint64_t)(uintptr_t)s2_l1;
    uint64_t guest_l2_address = (uint64_t)(uintptr_t)s2_guest_l2;
    uint64_t mmio_l2_address = (uint64_t)(uintptr_t)s2_mmio_l2;
    uint64_t gicv_l3_address = (uint64_t)(uintptr_t)s2_gicv_l3;
    uint64_t vtcr;
    uint64_t hcr;

    zero_table(s2_l1);
    zero_table(s2_guest_l2);
    zero_table(s2_mmio_l2);
    zero_table(s2_gicv_l3);

    s2_l1[0] = (guest_l2_address & S2_ADDRESS_MASK) |
               S2_DESC_TABLE | S2_DESC_VALID;
    s2_l1[((uint64_t)PI400_GICV_BASE >> 30) & UINT64_C(0x1ff)] =
        (mmio_l2_address & S2_ADDRESS_MASK) |
        S2_DESC_TABLE | S2_DESC_VALID;

    guest_attributes = S2_MEMATTR_NORMAL | S2_S2AP_READ | S2_S2AP_WRITE |
                       S2_SH_INNER | S2_ACCESS_FLAG;
    s2_guest_l2[GUEST_IPA_BASE >> 21] =
        GUEST_IPA_BASE | guest_attributes | S2_DESC_VALID;

    s2_mmio_l2[((uint64_t)PI400_GICV_BASE >> 21) & UINT64_C(0x1ff)] =
        (gicv_l3_address & S2_ADDRESS_MASK) |
        S2_DESC_TABLE | S2_DESC_VALID;

    gicv_attributes = S2_MEMATTR_DEVICE_NGNRE | S2_S2AP_READ |
                      S2_S2AP_WRITE | S2_SH_INNER | S2_ACCESS_FLAG |
                      S2_EXECUTE_NEVER;
    s2_gicv_l3[((uint64_t)PI400_GICV_BASE >> 12) & UINT64_C(0x1ff)] =
        PI400_GICV_BASE | gicv_attributes |
        S2_DESC_TABLE | S2_DESC_VALID;
    s2_gicv_l3[(((uint64_t)PI400_GICV_BASE >> 12) & UINT64_C(0x1ff)) + 1] =
        ((uint64_t)PI400_GICV_BASE + GICV_DIR) |
        gicv_attributes | S2_DESC_TABLE | S2_DESC_VALID;

    vtcr = VTCR_RES1 | VTCR_SH0_INNER | VTCR_ORGN0_WBWA |
           VTCR_IRGN0_WBWA | VTCR_SL0_LEVEL1 | VTCR_T0SZ_32BIT_IPA;

    __asm__ volatile(
        "dsb sy\n"
        "msr VTCR_EL2, %0\n"
        "msr VTTBR_EL2, %1\n"
        "isb\n"
        "tlbi VMALLS12E1IS\n"
        "dsb ish\n"
        "isb\n"
        :
        : "r"(vtcr), "r"(root_address)
        : "memory");

    hcr = HCR_RW | HCR_IMO | HCR_VM;
    __asm__ volatile(
        "msr HCR_EL2, %0\n"
        "isb\n"
        :
        : "r"(hcr)
        : "memory");
}

uint64_t stage2_root_address(void)
{
    return (uint64_t)(uintptr_t)s2_l1;
}

uint64_t stage2_guest_descriptor(void)
{
    return s2_guest_l2[GUEST_IPA_BASE >> 21];
}

uint64_t stage2_gicv_descriptor(void)
{
    return s2_gicv_l3[((uint64_t)PI400_GICV_BASE >> 12) &
                      UINT64_C(0x1ff)];
}

uint64_t stage2_gicv_dir_descriptor(void)
{
    return s2_gicv_l3[(((uint64_t)PI400_GICV_BASE >> 12) &
                       UINT64_C(0x1ff)) + 1];
}
