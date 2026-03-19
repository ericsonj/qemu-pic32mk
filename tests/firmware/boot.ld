/*
 * boot.ld — linker script for PIC32MK Phase 1 smoke-test firmware
 *
 * Places .text at Boot Flash 1 (KSEG0 cached alias 0x9FC40000).
 * RAM stack is set up in boot.S at KSEG1 0xA0000000.
 */

OUTPUT_ARCH(mips)
ENTRY(_reset)

MEMORY {
    BFLASH1 (rx)  : ORIGIN = 0x9FC40000, LENGTH = 20K
    RAM     (rwx) : ORIGIN = 0xA0000000, LENGTH = 256K
}

SECTIONS {
    .text : {
        *(.text*)
    } > BFLASH1

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM
}
