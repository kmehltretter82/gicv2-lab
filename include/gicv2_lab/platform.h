/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GICV2_LAB_PLATFORM_H
#define GICV2_LAB_PLATFORM_H

#include <stdint.h>

#include "gicv2_lab/gicv2_defs.h"

#define PI400_PL011_BASE UINT64_C(0xfe201000)

static inline uint32_t mmio_read32(uint64_t address)
{
    return *(volatile const uint32_t *)(uintptr_t)address;
}

static inline void mmio_write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint64_t read_currentel(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
    return value;
}

static inline uint64_t read_midr_el1(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, MIDR_EL1" : "=r"(value));
    return value;
}

static inline uint64_t read_mpidr_el1(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, MPIDR_EL1" : "=r"(value));
    return value;
}

static inline uint64_t read_cntfrq_el0(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(value));
    return value;
}

static inline uint64_t read_id_aa64pfr0_el1(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, ID_AA64PFR0_EL1" : "=r"(value));
    return value;
}

static inline uint64_t read_hcr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, HCR_EL2" : "=r"(value));
    return value;
}

static inline void write_hcr_el2(uint64_t value)
{
    __asm__ volatile("msr HCR_EL2, %0\n"
                     "isb\n"
                     : : "r"(value) : "memory");
}

static inline uint64_t read_cntpct_el0(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(value));
    return value;
}

static inline uint64_t read_cnthp_ctl_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CNTHP_CTL_EL2" : "=r"(value));
    return value;
}

static inline void write_cnthp_ctl_el2(uint64_t value)
{
    __asm__ volatile("msr CNTHP_CTL_EL2, %0\n"
                     "isb\n"
                     : : "r"(value) : "memory");
}

static inline void write_cnthp_tval_el2(uint64_t value)
{
    __asm__ volatile("msr CNTHP_TVAL_EL2, %0\n"
                     "isb\n"
                     : : "r"(value) : "memory");
}

static inline uint64_t read_vtcr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, VTCR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_vttbr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, VTTBR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_esr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, ESR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_far_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, FAR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_hpfar_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, HPFAR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_elr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, ELR_EL2" : "=r"(value));
    return value;
}

static inline uint64_t read_spsr_el2(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, SPSR_EL2" : "=r"(value));
    return value;
}

static inline void write_elr_el2(uint64_t value)
{
    __asm__ volatile("msr ELR_EL2, %0" : : "r"(value));
}

static inline void cpu_relax(void)
{
    __asm__ volatile("wfe");
}

#endif
