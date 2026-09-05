/* SPDX-License-Identifier: GPL-2.0-or-later */
.section .text.boot, "ax"
.global _start
_start:
    msr daifset, #0xf
    ldr x0, =_stack_top
    mov sp, x0
    adr x0, vectors
    msr vbar_el1, x0
    isb
    bl h7_main
1:  wfe
    b 1b

.balign 2048
vectors:
.rept 16
    b h7_exception
    .balign 128
.endr
