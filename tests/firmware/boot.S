/*
 * boot.S — minimal PIC32MK Phase 1 smoke-test firmware
 *
 * Assemble & link:
 *   mipsel-linux-gnu-gcc -march=mips32r2 -EL -nostdlib -T boot.ld \
 *       boot.S -o boot.elf
 *   mipsel-linux-gnu-objcopy -O binary boot.elf boot.bin
 *
 * Run in QEMU:
 *   ./qemu-system-mipsel -M pic32mk -bios tests/firmware/boot.bin \
 *       -d unimp,guest_errors -nographic -serial stdio
 *
 * Memory layout:
 *   Boot Flash 1 physical:  0x1FC40000  (KSEG0: 0x9FC40000, KSEG1: 0xBFC40000)
 *   RAM physical:           0x00000000  (KSEG1: 0xA0000000, size 256 KB)
 *   Reset vector:           0xBFC00000  -> jumps to Boot Flash 1
 *
 * The MIPS reset vector at 0xBFC00000 is in the pre-boot region. The linker
 * script places _reset at 0x9FC40000 (Boot Flash 1, KSEG0 cached).
 * A small trampoline at 0xBFC00000 would normally be needed, but since QEMU
 * loads the binary directly into Boot Flash 1 and the PC starts at
 * 0xBFC00000, we start execution from the top of the binary. Placing the
 * binary at BFLASH1 (0x1FC40000) means the CPU fetches from there when it
 * reads 0xBFC40000 — but the actual reset vector is 0xBFC00000 (physical
 * 0x1FC00000), which is before Boot Flash 1.
 *
 * For QEMU testing, use -bios to load into BFLASH1 and observe the
 * LOG_UNIMP output for unimplemented SFR accesses.
 */

    .section .text
    .set    noreorder
    .set    mips32r2
    .org    0x0

_reset:
    /* Disable all interrupts, enter kernel mode */
    mtc0    $zero, $12          /* CP0 Status = 0 (IE=0, EXL=0, ERL=0) */
    nop

    /* Set K0 cache attribute = 3 (cacheable, write-back, write-allocate) */
    mfc0    $t0, $16            /* CP0 Config */
    ins     $t0, $zero, 0, 3   /* Clear K0 field [2:0] */
    ori     $t0, $t0, 3        /* K0 = 3 */
    mtc0    $t0, $16
    nop

    /* Initialise stack pointer to top of KSEG1 RAM (uncached)
     * Physical RAM top: 0x00000000 + 256KB = 0x00040000
     * KSEG1 alias:      0xA0040000 */
    lui     $sp, 0xA004
    addiu   $sp, $sp, -8       /* 8-byte aligned */

    /* Zero BSS (none defined here, placeholder for future firmware) */

    /* Spin — clean halting state; QEMU -d unimp shows SFR accesses */
_loop:
    wait                        /* MIPS32R2 wait (low-power idle) */
    b       _loop
    nop
