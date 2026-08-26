/*
 * FlintRTOS - Raspberry Pi 4 (BCM2711, AArch64) link script.
 * The GPU firmware loads kernel8.img to 0x80000 and enters at _start.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x80000;                /* AArch64 kernel load address on RPi */

    .text : {
        KEEP(*(.text.boot))     /* _start must be first */
        *(.text .text.*)
    }

    .rodata : { *(.rodata .rodata.*) }

    .data : { *(.data .data.*) }

    /* 8-byte aligned BSS, zeroed by start.S using __bss_start/__bss_end. */
    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(16);
    __bss_end = .;

    /* Initial (boot) stack top grows down from here. */
    . = ALIGN(4096);
    . = . + 0x10000;            /* 64 KiB boot stack */
    __boot_stack_top = .;

    __image_end = .;

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
