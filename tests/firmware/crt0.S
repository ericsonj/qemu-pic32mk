/*
 * crt0.S — minimal C runtime for PIC32MK QEMU firmware
 *
 * Responsibilities:
 *   1. Disable interrupts
 *   2. Set K0 cache attribute
 *   3. Initialise stack pointer
 *   4. Zero the BSS section
 *   5. Call main()
 *   6. Spin forever if main() returns
 */

    .section .text.startup
    .set    noreorder
    .set    mips32r2
    .global _start
    .type   _start, @function

_start:
    /* --- 1. Disable all interrupts, enter kernel mode --- */
    mtc0    $zero, $12          /* CP0 Status = 0 (IE=0, EXL=0, ERL=0) */
    nop

    /* --- 2. Set K0 = 3 (cacheable write-back, write-allocate) --- */
    mfc0    $t0, $16            /* CP0 Config */
    ins     $t0, $zero, 0, 3   /* clear K0 field [2:0] */
    ori     $t0, $t0, 3
    mtc0    $t0, $16
    nop

    /* --- 3. Stack pointer = top of KSEG1 RAM (uncached) ---
     * Physical RAM: 0x00000000, size 256 KB = 0x00040000
     * KSEG1 top:    0xA0040000 - 8 (8-byte aligned)           */
    lui     $sp, 0xA004
    addiu   $sp, $sp, -8

    /* --- 4. Zero BSS --- */
    la      $t0, __bss_start
    la      $t1, __bss_end
_bss_loop:
    beq     $t0, $t1, _bss_done
    nop
    sw      $zero, 0($t0)
    addiu   $t0, $t0, 4
    b       _bss_loop
    nop
_bss_done:

    /* --- 5. Call main() --- */
    jal     main
    nop

    /* --- 6. Spin if main() returns --- */
_hang:
    b       _hang
    nop

    .size _start, . - _start
