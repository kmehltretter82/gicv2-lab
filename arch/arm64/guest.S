/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/hvc.h"
#include "gicv2_lab/gicv2_defs.h"
#include "gicv2_lab/layout.h"

.section .text.guest_entry, "ax"
.align 4
.global enter_guest
.type enter_guest, %function

enter_guest:
    msr elr_el2, x0
    msr sp_el1, x1

    mrs x2, sctlr_el1
    bic x2, x2, #1
    bic x2, x2, #4
    bic x2, x2, #0x1000
    msr sctlr_el1, x2

    mov x2, #0x3c5
    msr spsr_el2, x2
    isb
    eret

.size enter_guest, . - enter_guest

.section .guest.text, "ax"
.align 12
.global guest_start
.type guest_start, %function

guest_start:
    adr x2, guest_vectors
    msr vbar_el1, x2
    isb

    adr x2, guest_irq_count
    str wzr, [x2]

    mrs x1, CurrentEL
    mov x0, #HVC_REPORT
    hvc #0

    mov x2, #MONITOR_LOAD_BASE
    ldr x3, [x2]

    mov x0, #HVC_PASS
    mov x1, #HVC_PASS_MAGIC
    hvc #0

    movz x2, #PI400_GICV_BASE_LO
    movk x2, #PI400_GICV_BASE_HI, lsl #16
    ldr w3, [x2, #GICV_CTLR]
    ldr w4, [x2, #GICV_PMR]
    ldr w5, [x2, #GICV_BPR]
    orr x1, x3, x4, lsl #32
    orr x1, x1, x5, lsl #40
    mov x0, #HVC_IRQ_READY
    hvc #0

    movz x2, #PI400_GICV_BASE_LO
    movk x2, #PI400_GICV_BASE_HI, lsl #16
    ldr w1, [x2, #GICV_HPPIR]
    mov x0, #HVC_REFILL_PENDING
    hvc #0

    msr daifclr, #2
    isb

    adr x2, guest_irq_count
1:
    ldr w3, [x2]
    cmp w3, #GICV2_REFILL_DELIVERIES
    b.ne 1b

    msr daifset, #2
    isb
    mov x0, #HVC_EXIT
    mov x1, xzr
    hvc #0

2:
    wfe
    b 2b

.size guest_start, . - guest_start

.section .guest.vectors, "ax"
.align 11
.global guest_vectors
.type guest_vectors, %function

.macro GUEST_VECTOR number, target
    sub sp, sp, #64
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    mrs x2, elr_el1
    mrs x3, spsr_el1
    stp x2, x3, [sp, #32]
    mov x1, #\number
    b \target
    .space 96
.endm

.macro REPORT_IRQ_STATE operation
    ldr w3, [x2, #GICV_RPR]
    mov w1, w1
    orr x1, x1, x3, lsl #32
    ldr w3, [x2, #GICV_HPPIR]
    orr x1, x1, x3, lsl #40
    dsb sy
    mov x0, #\operation
    hvc #0
.endm

guest_vectors:
    GUEST_VECTOR 0, guest_unexpected
    GUEST_VECTOR 1, guest_unexpected
    GUEST_VECTOR 2, guest_unexpected
    GUEST_VECTOR 3, guest_unexpected
    GUEST_VECTOR 4, guest_unexpected
    GUEST_VECTOR 5, guest_irq_entry
    GUEST_VECTOR 6, guest_unexpected
    GUEST_VECTOR 7, guest_unexpected
    GUEST_VECTOR 8, guest_unexpected
    GUEST_VECTOR 9, guest_unexpected
    GUEST_VECTOR 10, guest_unexpected
    GUEST_VECTOR 11, guest_unexpected
    GUEST_VECTOR 12, guest_unexpected
    GUEST_VECTOR 13, guest_unexpected
    GUEST_VECTOR 14, guest_unexpected
    GUEST_VECTOR 15, guest_unexpected

guest_irq_entry:
    adr x2, guest_irq_count
    ldr w3, [x2]
    cmp w3, #GICV2_REFILL_DELIVERIES
    b.hs guest_irq_bad

    movz x2, #PI400_GICV_BASE_LO
    movk x2, #PI400_GICV_BASE_HI, lsl #16
    ldr w1, [x2, #GICV_IAR]
    add w0, w3, #GICV2_REFILL_FIRST_INTID
    cmp w1, w0
    b.ne guest_irq_bad

    REPORT_IRQ_STATE HVC_REFILL_ACTIVE

    str w1, [x2, #GICV_EOIR]
    dsb sy
    REPORT_IRQ_STATE HVC_REFILL_EOI

    str w1, [x2, #GICV_DIR]
    dsb sy
    REPORT_IRQ_STATE HVC_REFILL_COMPLETE

    adr x2, guest_irq_count
    ldr w3, [x2]
    add w3, w3, #1
    str w3, [x2]
    dsb sy

guest_irq_return:
    ldp x2, x3, [sp, #32]
    msr elr_el1, x2
    msr spsr_el1, x3
    ldp x2, x3, [sp, #16]
    ldp x0, x1, [sp, #0]
    add sp, sp, #64
    eret

guest_irq_bad:
    mov x0, #HVC_FAIL
    hvc #0
    b guest_halt

guest_unexpected:
    mov x0, #HVC_FAIL
    hvc #0

guest_halt:
    wfe
    b guest_halt

.size guest_vectors, . - guest_vectors

.section .guest.data, "aw"
.align 2
.global guest_irq_count
.type guest_irq_count, %object
guest_irq_count:
    .word 0
.size guest_irq_count, . - guest_irq_count
