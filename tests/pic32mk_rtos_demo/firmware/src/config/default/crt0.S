/*
 * crt0.S — PIC32MK FreeRTOS startup + exception vector stubs
 *
 * Firmware loads at 0xBFC40000 (Boot Flash 1, KSEG1 uncached).
 * We set EBASE = 0xBFC40000.  The .vectors section must be placed at
 * exactly 0xBFC40000 by the linker script so that:
 *
 *   offset 0x000  _start         (reset entry, also EBASE+0x000)
 *   offset 0x180  _gen_exception (EBASE+0x180, general exception)
 *   offset 0x200  _int_handler   (EBASE+0x200, interrupt exception)
 *   offset 0x400  _crt_init      (BSS zero, data copy, call main)
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <xc.h>
#include "FreeRTOSConfig.h"

    .set    nomips16
    .set    nomicromips
    .set    noreorder
    .set    noat

/* ============================================================
 * Everything in one section so .org offsets are absolute
 * within the section (which the linker places at 0xBFC40000).
 * ============================================================ */
    .section .vectors, "ax", @progbits

/* ----------------------------------------------------------
 * EBASE + 0x000 — reset / TLB-miss entry
 * ---------------------------------------------------------- */
    .global _start
    .ent    _start
_start:
    /* STATUS = 0 → disable all interrupts */
    mtc0    $0, _CP0_STATUS
    ehb

    /* CP0_EBASE = 0xBFC40000 (this flash block, KSEG1 uncached) */
    lui     $8, 0xBFC4
    mtc0    $8, _CP0_EBASE
    ehb

    /* CP0_Cause.IV = 1 → use special interrupt vector (EBASE+0x200).
     * In VEIC mode the EIC provides vector offsets, but QEMU still
     * requires IV=1 for its exception dispatch to reach EBASE+0x200
     * (see tlb_helper.c EXCP_EXT_INTERRUPT handler). */
    lui     $8, 0x0080          /* bit 23 = IV */
    mtc0    $8, _CP0_CAUSE
    ehb

    /* IntCtl.VS = 0 → all interrupts land at EBASE+0x200 (single
     * vector mode).  With VS=0, _int_handler does the dispatch.
     * A non-zero VS would enable per-vector offsets via the EVIC's
     * OFFx registers, but that requires vector stubs at each offset. */
    mfc0    $8, _CP0_INTCTL
    ins     $8, $0, 5, 5        /* clear VS field [9:5] */
    mtc0    $8, _CP0_INTCTL
    ehb

    /* STATUS: MX=1 (bit 24, enables DSP ASE), IPL=0, IE=0.
     * In VEIC mode (Config3.VEIC=1), Status[17:10] is the IPL
     * priority field — NOT individual IM mask bits.
     * IPL=0 accepts all interrupt priorities once IE is set.
     * BEV=0 so exceptions use EBASE, not the bootstrap vectors. */
    lui     $8, 0x0100          /* bit 24 = MX */
    mtc0    $8, _CP0_STATUS
    ehb

    /* CONFIG.K0 = 3 (cacheable writeback for KSEG0) */
    mfc0    $8, $16, 0
    ins     $8, $0, 0, 3
    ori     $8, $8, 3
    mtc0    $8, $16, 0
    ehb

    /* Jump to C runtime init (past the vector stubs below) */
    j       _crt_init
    nop
    .end    _start

/* ----------------------------------------------------------
 * EBASE + 0x180 — general exception handler
 * Reached for address errors, bus errors, syscall, etc.
 * Just hang for now.
 * ---------------------------------------------------------- */
    .org    0x180
    .global _gen_exception
    .ent    _gen_exception
_gen_exception:
1:  wait
    b       1b
    nop
    .end    _gen_exception

/* ----------------------------------------------------------
 * EBASE + 0x200 — interrupt exception handler
 *
 * Dispatch:
 *   IFS0.CS0IF  (bit 1) & IEC0  -> vPortYieldISR  (software yield request)
 *   IFS1.U1TXIF (bit 8) & IEC1  -> vUART1TXInterruptWrapper
 *   IFS1.U1RXIF (bit 7) & IEC1  -> vUART1RXInterruptWrapper
 *   IFS1.U1EIF  (bit 6) & IEC1  -> vUART1FaultInterruptWrapper
 *   otherwise                    -> vPortTickInterruptHandler  (hw interrupt)
 *
 * UART1 sources are in IFS1/IEC1 (EVIC base + 0x50/0xD0):
 *   U1E  = source 38 = IFS1 bit 6
 *   U1RX = source 39 = IFS1 bit 7
 *   U1TX = source 40 = IFS1 bit 8
 * ---------------------------------------------------------- */
    .org    0x200
    .global _int_handler
    .ent    _int_handler
_int_handler:
    /* --- Check CS0 (yield) in IFS0/IEC0 --- */
    lui     $26, 0xBF81             /* EVIC base = 0xBF810000 */
    lw      $27, 0x0040($26)        /* $27 = IFS0 */
    lw      $26, 0x00C0($26)        /* $26 = IEC0 */
    and     $26, $27, $26           /* pending0 = IFS0 & IEC0 */
    andi    $26, $26, 0x2           /* CS0IF (bit 1) */
    bnez    $26, _sw_yield
    nop

    /* --- Check UART1 interrupts in IFS1/IEC1 --- */
    lui     $26, 0xBF81             /* reload EVIC base */
    lw      $27, 0x0050($26)        /* $27 = IFS1 */
    lw      $26, 0x00D0($26)        /* $26 = IEC1 */
    and     $26, $27, $26           /* pending1 = IFS1 & IEC1 */

    andi    $27, $26, 0x100         /* U1TXIF (bit 8) */
    bnez    $27, _uart1_tx_int
    nop

    andi    $27, $26, 0x80          /* U1RXIF (bit 7) */
    bnez    $27, _uart1_rx_int
    nop

    andi    $27, $26, 0x40          /* U1EIF (bit 6) */
    bnez    $27, _uart1_fault_int
    nop

    andi    $27, $26, 0x4           /* USB1IF (source 34 = IFS1 bit 2) */
    bnez    $27, _usb1_int
    nop

    /* CNA = source 44 = IFS1 bit 12 = 0x1000 */
    andi    $27, $26, 0x1000        /* CNAIF (bit 12) */
    bnez    $27, _cna_int
    nop

    /* --- Check UART2 interrupts in IFS3/IEC3 ---
     * UART2: U2E=source 115 (IFS3 bit 19), U2RX=116 (bit 20), U2TX=117 (bit 21)
     * IFS3 at EVIC+0x0070, IEC3 at EVIC+0x00F0 */
    lui     $26, 0xBF81             /* reload EVIC base */
    lw      $27, 0x0070($26)        /* $27 = IFS3 */
    lw      $26, 0x00F0($26)        /* $26 = IEC3 */
    and     $26, $27, $26           /* pending3 = IFS3 & IEC3 */

    /* AD1EOS = source 101 = IFS3 bit 5 = 0x0020 */
    andi    $27, $26, 0x0020        /* AD1EOSIF (bit 5) */
    bnez    $27, _adc_eos_int
    nop

    lui     $27, 0x0020             /* bit 21 = 0x00200000 (U2TXIF) */
    and     $27, $26, $27
    bnez    $27, _uart2_tx_int
    nop

    lui     $27, 0x0010             /* bit 20 = 0x00100000 (U2RXIF) */
    and     $27, $26, $27
    bnez    $27, _uart2_rx_int
    nop

    lui     $27, 0x0008             /* bit 19 = 0x00080000 (U2EIF) */
    and     $27, $26, $27
    bnez    $27, _uart2_fault_int
    nop

    /* --- Check CAN1 interrupt in IFS5/IEC5 ---
     * CAN1 = EVIC source 167 = IFS5 bit 7
     * IFS5 at EVIC+0x0090, IEC5 at EVIC+0x0110 */
    lui     $26, 0xBF81             /* reload EVIC base */
    lw      $27, 0x0090($26)        /* $27 = IFS5 */
    lw      $26, 0x0110($26)        /* $26 = IEC5 */
    and     $26, $27, $26           /* pending5 = IFS5 & IEC5 */

    andi    $27, $26, 0x80          /* CAN1IF (bit 7) */
    bnez    $27, _can1_int
    nop

    /* --- Check CAN2 interrupt in IFS5/IEC5 ---
     * CAN2 = EVIC source 168 = IFS5 bit 8 */
    lui     $26, 0xBF81             /* reload EVIC base */
    lw      $27, 0x0090($26)        /* $27 = IFS5 */
    lw      $26, 0x0110($26)        /* $26 = IEC5 */
    and     $26, $27, $26           /* pending5 = IFS5 & IEC5 */

    andi    $27, $26, 0x100         /* CAN2IF (bit 8) */
    bnez    $27, _can2_int
    nop

    /* --- Default: Timer1 tick or other hardware interrupt --- */
    .extern vPortTickInterruptHandler
    la      $26, vPortTickInterruptHandler
    jr      $26
    nop

_sw_yield:
    /* Clear CS0IF in the EVIC before entering the yield handler.
     * On real PIC32MK, CS0 is a Cause-register software interrupt.
     * In our QEMU EVIC redirect, it sets IFS0.CS0IF which must be
     * explicitly cleared to prevent infinite re-entry. */
    lui     $26, 0xBF81             /* EVIC base */
    li      $27, 0x2                /* CS0IF = bit 1 */
    sw      $27, 0x0044($26)        /* IFS0CLR (base+4) = CS0IF mask */

    .extern vPortYieldISR
    la      $26, vPortYieldISR
    jr      $26
    nop

_uart1_tx_int:
    .extern vUART1TXInterruptWrapper
    la      $26, vUART1TXInterruptWrapper
    jr      $26
    nop

_uart1_rx_int:
    .extern vUART1RXInterruptWrapper
    la      $26, vUART1RXInterruptWrapper
    jr      $26
    nop

_uart1_fault_int:
    .extern vUART1FaultInterruptWrapper
    la      $26, vUART1FaultInterruptWrapper
    jr      $26
    nop

_usb1_int:
    .extern vUSB1InterruptWrapper
    la      $26, vUSB1InterruptWrapper
    jr      $26
    nop

_cna_int:
    .extern vCNAInterruptWrapper
    la      $26, vCNAInterruptWrapper
    jr      $26
    nop

_uart2_tx_int:
    .extern vUART2TXInterruptWrapper
    la      $26, vUART2TXInterruptWrapper
    jr      $26
    nop

_uart2_rx_int:
    .extern vUART2RXInterruptWrapper
    la      $26, vUART2RXInterruptWrapper
    jr      $26
    nop

_uart2_fault_int:
    .extern vUART2FaultInterruptWrapper
    la      $26, vUART2FaultInterruptWrapper
    jr      $26
    nop

_can1_int:
    .extern vCAN1InterruptWrapper
    la      $26, vCAN1InterruptWrapper
    jr      $26
    nop

_can2_int:
    .extern vCAN2InterruptWrapper
    la      $26, vCAN2InterruptWrapper
    jr      $26
    nop

_adc_eos_int:
    .extern vADCEOSInterruptWrapper
    la      $26, vADCEOSInterruptWrapper
    jr      $26
    nop
    .end    _int_handler

/* ----------------------------------------------------------
 * EBASE + 0x400 — C runtime initialisation
 * ---------------------------------------------------------- */
    .org    0x400
    .global _crt_init
    .ent    _crt_init
_crt_init:
    /* Stack pointer: top of KSEG1 RAM (0xA0000000 + 256 KB - 8) */
    lui     $29, 0xA004
    addiu   $29, $29, -8        /* sp = 0xA003FFF8 */

    /* Global pointer */
    la      $28, _gp

    /* Zero BSS */
    la      $8, _bss_start
    la      $9, _bss_end
    beq     $8, $9, .Ldata
    nop
.Lbss:
    sw      $0, 0($8)
    addiu   $8, $8, 4
    bne     $8, $9, .Lbss
    nop

    /* Copy .data from flash (LMA) to RAM (VMA) */
.Ldata:
    la      $8, _data_lma
    la      $9, _data_start
    la      $10, _data_end
    beq     $9, $10, .Lmain
    nop
.Lcopy:
    lw      $11, 0($8)
    sw      $11, 0($9)
    addiu   $8, $8, 4
    addiu   $9, $9, 4
    bne     $9, $10, .Lcopy
    nop

.Lmain:
    jal     main
    nop

.Lhang:
    b       .Lhang
    nop
    .end    _crt_init
