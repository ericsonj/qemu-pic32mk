/*
 * link.ld — PIC32MK FreeRTOS Hello World linker script
 *
 * Boot Flash 1: 256 KB, physical 0x1FC40000, KSEG1 virtual 0xBFC40000.
 * EBASE = 0xBFC40000, so exception vectors reside within this flash:
 *   offset 0x000 = reset / TLB refill
 *   offset 0x180 = general exception
 *   offset 0x200 = interrupt
 *   offset 0x400 = code starts (past vector stubs)
 *
 * RAM: 256 KB, physical 0x00000000, KSEG1 virtual 0xA0000000.
 */

OUTPUT_ARCH(mips)
ENTRY(_start)

MEMORY {
    flash  (rx)  : ORIGIN = 0xBFC40000, LENGTH = 256K
    ram    (rwx) : ORIGIN = 0xA0000000, LENGTH = 256K
}

SECTIONS {
    /* Vector stubs and all code in one contiguous flash section.
     * .org inside crt0.S positions handlers at correct EBASE offsets. */
    .vectors 0xBFC40000 : {
        KEEP(*(.vectors))      /* crt0.S .vectors section */
        *(.vector*)            /* FreeRTOS port_asm .vector_N stubs */
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
        _etext = .;
    } > flash

    /* .data: LMA in flash immediately after text, VMA in RAM */
    _data_lma = LOADADDR(.data);

    .data : {
        _data_start = .;
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(4);
        _data_end = .;
    } > ram AT > flash

    /* .bss: uninitialised data in RAM */
    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > ram

    _gp = ALIGN(16);
    _stack_top = ORIGIN(ram) + LENGTH(ram) - 8;

    /DISCARD/ : {
        *(.MIPS.abiflags)
        *(.reginfo)
        *(.note*)
        *(.comment)
        *(.pdr)
    }
}
