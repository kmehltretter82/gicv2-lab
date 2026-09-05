/* SPDX-License-Identifier: GPL-2.0-or-later */
OUTPUT_ARCH(aarch64)
ENTRY(_start)
MEMORY { ram (rwx) : ORIGIN = 0x40000000, LENGTH = 1M }
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
    .text : { KEEP(*(.text.boot)) *(.text .text.*) } > ram :text
    .rodata : { *(.rodata .rodata.*) } > ram :text
    .data : { *(.data .data.*) } > ram :data
    .bss (NOLOAD) : { *(.bss .bss.* COMMON) } > ram :data
    ASSERT(SIZEOF(.bss) == 0, "H7 guest must not require mutable globals")
    _stack_top = ORIGIN(ram) + LENGTH(ram);
    ASSERT(. < _stack_top - 0x4000, "H7 image overlaps stack")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
