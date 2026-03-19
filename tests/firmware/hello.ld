/*
 * hello.ld — linker script for PIC32MK Hello World firmware
 *
 * Text/rodata → Boot Flash 1 (KSEG1 0xBFC40000, physical 0x1FC40000)
 * BSS         → KSEG1 RAM   (0xA0000000, physical 0x00000000)
 *
 * The QEMU trampoline at 0xBFC00000 jumps to 0xBFC40000, so the
 * firmware entry point must be linked at that address.
 */

OUTPUT_ARCH(mips)
ENTRY(_start)

MEMORY {
    BFLASH1 (rx)  : ORIGIN = 0xBFC40000, LENGTH = 20K
    RAM     (rwx) : ORIGIN = 0xA0000000, LENGTH = 256K
}

SECTIONS {
    .text : {
        *(.text.startup) /* crt0 first */
        *(.text*)
        *(.rodata*)      /* string literals stay in flash */
    } > BFLASH1

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
}
