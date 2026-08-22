/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gicv2_lab/hvc.h"
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
    mrs x1, CurrentEL
    mov x0, #HVC_REPORT
    hvc #0

    mov x2, #MONITOR_LOAD_BASE
    ldr x3, [x2]

    mov x0, #HVC_PASS
    mov x1, #HVC_PASS_MAGIC
    hvc #0

    mov x0, #HVC_EXIT
    mov x1, xzr
    hvc #0

1:
    wfe
    b 1b

.size guest_start, . - guest_start
