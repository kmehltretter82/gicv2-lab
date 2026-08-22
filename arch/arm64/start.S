/* SPDX-License-Identifier: GPL-2.0-or-later */

.section .text.boot, "ax"
.align 7
.global _start
.type _start, %function

_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, 1f

0:
    wfe
    b 0b

1:
    msr daifset, #0xf

    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, xzr
2:
    cmp x0, x1
    b.hs 3f
    str x2, [x0], #8
    b 2b

3:
    msr spsel, #1
    ldr x0, =__stack_top
    mov sp, x0

    ldr x0, =vectors
    msr vbar_el2, x0
    isb

    bl lab_main

4:
    wfe
    b 4b

.size _start, . - _start
