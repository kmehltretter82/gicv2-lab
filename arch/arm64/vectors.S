/* SPDX-License-Identifier: GPL-2.0-or-later */

.section .text.vectors, "ax"
.align 11
.global vectors
.type vectors, %function

.macro VECTOR_SLOT number
    sub sp, sp, #256
    stp x0, x1, [sp, #0]
    mov x0, #\number
    b exception_entry
    .space 112
.endm

vectors:
    VECTOR_SLOT 0
    VECTOR_SLOT 1
    VECTOR_SLOT 2
    VECTOR_SLOT 3
    VECTOR_SLOT 4
    VECTOR_SLOT 5
    VECTOR_SLOT 6
    VECTOR_SLOT 7
    VECTOR_SLOT 8
    VECTOR_SLOT 9
    VECTOR_SLOT 10
    VECTOR_SLOT 11
    VECTOR_SLOT 12
    VECTOR_SLOT 13
    VECTOR_SLOT 14
    VECTOR_SLOT 15

exception_entry:
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    str x0, [sp, #248]

    mov x0, sp
    bl exception_dispatch
    cbz x0, 2f

    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    ldp x0, x1, [sp, #0]
    add sp, sp, #256
    eret

2:
    wfe
    b 2b

.size vectors, . - vectors
