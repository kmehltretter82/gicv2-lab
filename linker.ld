ENTRY(_start)

SECTIONS
{
    . = 0x00080000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.boot))
        . = ALIGN(2048);
        KEEP(*(.text.vectors))
        *(.text .text.*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(16)
    {
        *(.data .data.*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __stack_bottom = .;
        . += 0x4000;
        __stack_top = .;
        __bss_end = .;
    }

    __monitor_end = .;
    ASSERT(__monitor_end < 0x00200000,
           "EL2 monitor overlaps the guest region")

    . = 0x00200000;

    .guest : ALIGN(4096)
    {
        __guest_start = .;
        KEEP(*(.guest.text))
        . = ALIGN(2048);
        KEEP(*(.guest.vectors))
        *(.guest.data)
        __guest_end = .;
    }

    ASSERT(__guest_end <= 0x003ff000,
           "EL1 guest overlaps its stack")

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }
}
