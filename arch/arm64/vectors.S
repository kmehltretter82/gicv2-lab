/* SPDX-License-Identifier: GPL-2.0-or-later */

.section .text.vectors, "ax"
.align 11
.global vectors
.type vectors, %function

.macro VECTOR_SLOT number
    mov x0, #\number
    b exception_entry
    .space 120
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
    bl exception_report

1:
    wfe
    b 1b

.size vectors, . - vectors
