/*
 * PIC32MK GPK/MCM with CAN FD — shared constants
 * Datasheet: DS60001519E
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MIPS_PIC32MK_H
#define HW_MIPS_PIC32MK_H

/* -----------------------------------------------------------------------
 * Physical memory map (§4, pp. 70-73)
 * ----------------------------------------------------------------------- */

#define PIC32MK_RAM_BASE        0x00000000u   /* 256 KB SRAM */
#define PIC32MK_RAM_SIZE        (256 * 1024)

#define PIC32MK_PFLASH_BASE     0x1D000000u   /* 1 MB Program Flash */
#define PIC32MK_PFLASH_SIZE     (1 * 1024 * 1024)

/*
 * Boot vector ROM — fills the gap between the MIPS reset vector (0x1FC00000)
 * and Boot Flash 1 (0x1FC40000). Contains a trampoline that jumps to BFlash1.
 */
#define PIC32MK_BOOTVEC_BASE    0x1FC00000u   /* physical reset vector */
#define PIC32MK_BOOTVEC_SIZE    0x00040000u   /* 256 KB gap to BFlash1 */

#define PIC32MK_BFLASH1_BASE    0x1FC40000u   /* Boot Flash 1 — enlarged for firmware */
#define PIC32MK_BFLASH1_SIZE    (256 * 1024)

#define PIC32MK_BFLASH2_BASE    0x1FC60000u   /* Boot Flash 2 ~20 KB */
#define PIC32MK_BFLASH2_SIZE    (20 * 1024)

#define PIC32MK_SFR_BASE        0x1F800000u   /* SFR window, 1 MB */
#define PIC32MK_SFR_SIZE        (1 * 1024 * 1024)

/* Reset vector (KSEG1 uncached alias of 0x1FC00000) */
#define PIC32MK_RESET_VECTOR    0xBFC00000u

/* -----------------------------------------------------------------------
 * SFR sub-block bases (offsets within the 1 MB SFR window, Table 4-2)
 * All addresses below are KSEG1 virtual (0xBF800000 + offset).
 * ----------------------------------------------------------------------- */

/* CFG / PMD / CACHE / NVM / WDT / CRU / PPS  0xBF800000 */
#define PIC32MK_CFG_OFFSET      0x000000u

/* EVIC                                         0xBF810000 */
#define PIC32MK_EVIC_OFFSET     0x010000u

/* DMA (shares page with EVIC)                  0xBF811000 */
#define PIC32MK_DMA_OFFSET      0x011000u

/* Timers / IC / OC / I2C1-2 / SPI1-2 / UART1-2 / PWM / QEI / CMP / CDAC1 */
#define PIC32MK_PER1_OFFSET     0x020000u   /* 0xBF820000, size 0xD000 */

/* I2C3-4 / SPI3-6 / UART3-6 / CDAC2-3         0xBF840000 */
#define PIC32MK_PER2_OFFSET     0x040000u

/* GPIO PORTA-PORTG                             0xBF860000 */
#define PIC32MK_GPIO_OFFSET     0x060000u

/* CAN1-4 / ADC                                 0xBF880000 */
#define PIC32MK_CAN_OFFSET      0x080000u

/* CAN1–4 SFR offsets from SFR base (0xBF880000–0xBF883FFF) */
#define PIC32MK_CAN1_OFFSET     0x080000u   /* physical 0x1F880000 */
#define PIC32MK_CAN2_OFFSET     0x081000u
#define PIC32MK_CAN3_OFFSET     0x082000u
#define PIC32MK_CAN4_OFFSET     0x083000u
#define PIC32MK_CAN_SFR_SIZE    0x1000u     /* 4 KB SFR block per instance */

/*
 * Message RAM physical base per instance.
 * Allocated just above the SFR window, beyond the ADC / USB blocks.
 * ⚠  Verify exact addresses from DS60001519E §4 before final integration.
 * Each instance gets 76 KB (75 KB data + alignment).
 */
#define PIC32MK_CAN1_MSGRAM_BASE 0x1F900000u
#define PIC32MK_CAN2_MSGRAM_BASE 0x1F913000u
#define PIC32MK_CAN3_MSGRAM_BASE 0x1F926000u
#define PIC32MK_CAN4_MSGRAM_BASE 0x1F939000u
#define PIC32MK_CAN_MSGRAM_SIZE  (75u * 1024u)  /* max 74 KB, rounded up */

/* USB OTG 1                                   0xBF889000 */
#define PIC32MK_USB1_OFFSET     0x089000u
#define PIC32MK_USB_OFFSET      PIC32MK_USB1_OFFSET   /* legacy alias */

/* USB OTG 2                                   0xBF88A000 */
#define PIC32MK_USB2_OFFSET     0x08A000u

/* RTCC                                         0xBF8C0000 */
#define PIC32MK_RTCC_OFFSET     0x0C0000u

/* -----------------------------------------------------------------------
 * Reset-control registers (§7, p. 114) — offsets from SFR base
 * ----------------------------------------------------------------------- */

#define PIC32MK_RCON_OFFSET     0x001240u   /* Reset Control */
#define PIC32MK_RSWRST_OFFSET   0x001250u   /* Software Reset trigger */
#define PIC32MK_RNMICON_OFFSET  0x001260u   /* NMI Control */
#define PIC32MK_PWRCON_OFFSET   0x001270u   /* Power Control */

/* RCON reset flags (§7 register map) */
#define PIC32MK_RCON_POR        (1u << 2)   /* Power-on Reset */
#define PIC32MK_RCON_BOR        (1u << 3)   /* Brown-out Reset */

/* -----------------------------------------------------------------------
 * EVIC registers (§8, p. 159) — offsets from 0xBF810000
 * ----------------------------------------------------------------------- */

#define PIC32MK_EVIC_INTCON     0x0000u     /* MVEC, TPC, INTxEP */
#define PIC32MK_EVIC_PRISS      0x0010u     /* Priority shadow reg select */
#define PIC32MK_EVIC_INTSTAT    0x0020u     /* Last IRQ serviced, SRIPL */
#define PIC32MK_EVIC_IPTMR      0x0030u     /* Interrupt proximity timer */
#define PIC32MK_EVIC_IFS0       0x0040u     /* Interrupt flag status [0..7] */
#define PIC32MK_EVIC_IEC0       0x00C0u     /* Interrupt enable control [0..7] */
#define PIC32MK_EVIC_IPC0       0x0140u     /* Interrupt priority control [0..63] */
#define PIC32MK_EVIC_OFF0       0x0540u     /* Vector address offsets [0..190] */

/* Number of interrupt sources / vectors.
 * The EVIC IFS/IEC registers span 8×32 = 256 bits; USB1 (244) and USB2 (246)
 * are in IFS7.  Use 256 to cover the full IPC/IFS table. */
#define PIC32MK_NUM_IRQ_SOURCES 256
#define PIC32MK_NUM_VECTORS     190

/* -----------------------------------------------------------------------
 * UART1 registers (§21, Table 21-2) — U1MODE base 0xBF828000
 * Offset from SFR base: 0x028000
 * ----------------------------------------------------------------------- */

#define PIC32MK_UART1_OFFSET    0x028000u   /* SFR offset → physical 0x1F828000 */
#define PIC32MK_UART1_SIZE      0x000050u   /* U1MODE..U1BRG+INV */

/* Register offsets within the UART block (same layout for all UARTs) */
#define PIC32MK_UxMODE          0x00u       /* Mode */
#define PIC32MK_UxSTA           0x10u       /* Status & control */
#define PIC32MK_UxTXREG         0x20u       /* TX data */
#define PIC32MK_UxRXREG         0x30u       /* RX data */
#define PIC32MK_UxBRG           0x40u       /* Baud rate */

/* U1STA bits relevant to TX polling */
#define PIC32MK_USTA_TRMT       (1u << 8)   /* TX shift register empty (1=empty) */
#define PIC32MK_USTA_UTXBF      (1u << 9)   /* TX buffer full (0=not full) */

/* -----------------------------------------------------------------------
 * CPU clock (§3)
 * ----------------------------------------------------------------------- */

#define PIC32MK_CPU_HZ          120000000u  /* 120 MHz max */

/* -----------------------------------------------------------------------
 * Interrupt source numbers (§8, Table 8-1, DS60001519E)
 * TODO: verify each number against the actual datasheet table.
 * ----------------------------------------------------------------------- */

#define PIC32MK_IRQ_CT          0    /* Core Timer */
#define PIC32MK_IRQ_CS0         1    /* Core Software Interrupt 0 */
#define PIC32MK_IRQ_CS1         2    /* Core Software Interrupt 1 */
#define PIC32MK_IRQ_INT0        3    /* External Interrupt 0 */
#define PIC32MK_IRQ_T1          4    /* Timer 1 */
#define PIC32MK_IRQ_T2          9    /* Timer 2 */
#define PIC32MK_IRQ_T3          14   /* Timer 3 */
#define PIC32MK_IRQ_T4          19   /* Timer 4 */
#define PIC32MK_IRQ_T5          24   /* Timer 5 */
#define PIC32MK_IRQ_T6          28   /* Timer 6 */
#define PIC32MK_IRQ_T7          32   /* Timer 7 */
#define PIC32MK_IRQ_T8          36   /* Timer 8 */
#define PIC32MK_IRQ_T9          40   /* Timer 9 */
/* UART1: error=38, RX=39, TX=40  (IFS1 bits 6/7/8, p32mk1024mcm100.h) */
#define PIC32MK_IRQ_U1E         38
#define PIC32MK_IRQ_U1RX        39
#define PIC32MK_IRQ_U1TX        40
#define PIC32MK_IRQ_U2E         115
#define PIC32MK_IRQ_U2RX        116
#define PIC32MK_IRQ_U2TX        117
#define PIC32MK_IRQ_U3E         118
#define PIC32MK_IRQ_U3RX        119
#define PIC32MK_IRQ_U3TX        120
#define PIC32MK_IRQ_U4E         121
#define PIC32MK_IRQ_U4RX        122
#define PIC32MK_IRQ_U4TX        123
#define PIC32MK_IRQ_U5E         124
#define PIC32MK_IRQ_U5RX        125
#define PIC32MK_IRQ_U5TX        126
#define PIC32MK_IRQ_U6E         127
#define PIC32MK_IRQ_U6RX        128
#define PIC32MK_IRQ_U6TX        129
/* CAN FD — single IRQ per instance (§8, Table 8-1, DS60001519E) */
#define PIC32MK_IRQ_CAN1        167
#define PIC32MK_IRQ_CAN2        168
#define PIC32MK_IRQ_CAN3        187
#define PIC32MK_IRQ_CAN4        188

/* USB OTG — interrupt vector numbers from XC32 p32mk1024mcm100.h
 * USB1: vector 34 → IFS1 bit 2, IPC8[18:16]
 * USB2: vector 244 → IFS7 bit 20, IPC61[2:0] */
#define PIC32MK_IRQ_USB1        34
#define PIC32MK_IRQ_USB2        244

/* DMA channels 0-7 */
#define PIC32MK_IRQ_DMA0        134
#define PIC32MK_IRQ_DMA1        135
#define PIC32MK_IRQ_DMA2        136
#define PIC32MK_IRQ_DMA3        137
#define PIC32MK_IRQ_DMA4        138
#define PIC32MK_IRQ_DMA5        139
#define PIC32MK_IRQ_DMA6        140
#define PIC32MK_IRQ_DMA7        141

/* -----------------------------------------------------------------------
 * Timer peripheral registers (§14, DS60001519E)
 * Offsets from PIC32MK_PER1_OFFSET (0xBF820000).
 * Each timer block is 0x200 bytes; register stride 0x10 (with SET/CLR/INV).
 * TODO: verify exact base offsets for PIC32MK GPK from Table 4-2.
 * ----------------------------------------------------------------------- */

/* Timer base offsets from SFR base */
#define PIC32MK_T1_OFFSET       0x020000u   /* Timer 1 (Type A, 16-bit) */
#define PIC32MK_T2_OFFSET       0x020200u   /* Timer 2 (Type B) */
#define PIC32MK_T3_OFFSET       0x020400u   /* Timer 3 (Type C) */
#define PIC32MK_T4_OFFSET       0x020600u   /* Timer 4 (Type B) */
#define PIC32MK_T5_OFFSET       0x020800u   /* Timer 5 (Type C) */
#define PIC32MK_T6_OFFSET       0x020A00u   /* Timer 6 (Type B) */
#define PIC32MK_T7_OFFSET       0x020C00u   /* Timer 7 (Type C) */
#define PIC32MK_T8_OFFSET       0x020E00u   /* Timer 8 (Type B) */
#define PIC32MK_T9_OFFSET       0x021000u   /* Timer 9 (Type C) */

#define PIC32MK_TIMER_BLOCK_SIZE    0x200u  /* per-timer SFR block */

/* Timer register offsets within block (with SET/CLR/INV at +4/+8/+C) */
#define PIC32MK_TxCON           0x00u   /* Control: ON, TCKPS, T32, TCS... */
#define PIC32MK_TMRx            0x10u   /* Current count */
#define PIC32MK_PRx             0x20u   /* Period register */

/* TxCON bits */
#define PIC32MK_TCON_ON         (1u << 15)  /* Timer ON */
#define PIC32MK_TCON_T32        (1u << 3)   /* 32-bit mode (Type B only) */
#define PIC32MK_TCON_TCS        (1u << 1)   /* Clock source select */
#define PIC32MK_TCON_TCKPS_MASK 0x0070u     /* Prescaler bits [6:4] */
#define PIC32MK_TCON_TCKPS_SHIFT    4

/* -----------------------------------------------------------------------
 * UART2–6 register base offsets from SFR base
 * UART1 is already defined as PIC32MK_UART1_OFFSET = 0x028000
 * TODO: verify exact offsets against DS60001519E Table 4-2.
 * ----------------------------------------------------------------------- */

#define PIC32MK_UART2_OFFSET    0x028200u   /* UART2 (PER1 block) */
#define PIC32MK_UART3_OFFSET    0x048400u   /* UART3 (PER2 block, 0xBF848400) */
#define PIC32MK_UART4_OFFSET    0x048600u   /* UART4 (0xBF848600) */
#define PIC32MK_UART5_OFFSET    0x048800u   /* UART5 (0xBF848800) */
#define PIC32MK_UART6_OFFSET    0x048A00u   /* UART6 (0xBF848A00) */

#define PIC32MK_UART_BLOCK_SIZE 0x200u      /* per-UART SFR block */

/* Additional UxSTA bits */
#define PIC32MK_USTA_URXDA      (1u << 0)   /* RX data available */
#define PIC32MK_USTA_OERR       (1u << 1)   /* Overrun error */
#define PIC32MK_USTA_FERR       (1u << 2)   /* Framing error */
#define PIC32MK_USTA_PERR       (1u << 3)   /* Parity error */
#define PIC32MK_USTA_RIDLE      (1u << 4)   /* Receiver idle */
#define PIC32MK_USTA_UTXEN      (1u << 10)  /* TX enable */
#define PIC32MK_USTA_UTXISEL1   (1u << 14)  /* TX interrupt select */
#define PIC32MK_USTA_URXEN      (1u << 12)  /* RX enable */
#define PIC32MK_USTA_URXISEL1   (1u << 6)   /* RX interrupt select */
#define PIC32MK_UMODE_ON        (1u << 15)  /* UART enable */

/* -----------------------------------------------------------------------
 * SPI peripheral registers (§23, DS60001519E)
 * TODO: verify exact base offsets.
 * ----------------------------------------------------------------------- */

#define PIC32MK_SPI1_OFFSET     0x021800u
#define PIC32MK_SPI2_OFFSET     0x021A00u
#define PIC32MK_SPI3_OFFSET     0x040800u
#define PIC32MK_SPI4_OFFSET     0x040A00u
#define PIC32MK_SPI5_OFFSET     0x040C00u
#define PIC32MK_SPI6_OFFSET     0x040E00u
#define PIC32MK_SPI_BLOCK_SIZE  0x200u

/* SPI register offsets within block */
#define PIC32MK_SPIxCON         0x00u
#define PIC32MK_SPIxSTAT        0x10u
#define PIC32MK_SPIxBUF         0x20u
#define PIC32MK_SPIxBRG         0x30u
#define PIC32MK_SPIxCON2        0x40u

/* -----------------------------------------------------------------------
 * I2C peripheral registers (§24, DS60001519E)
 * TODO: verify exact base offsets.
 * ----------------------------------------------------------------------- */

#define PIC32MK_I2C1_OFFSET     0x022000u
#define PIC32MK_I2C2_OFFSET     0x022200u
#define PIC32MK_I2C3_OFFSET     0x040000u
#define PIC32MK_I2C4_OFFSET     0x040200u   /* NOTE: may conflict with UART3 */
#define PIC32MK_I2C_BLOCK_SIZE  0x200u

/* I2C register offsets within block */
#define PIC32MK_I2CxCON        0x00u
#define PIC32MK_I2CxSTAT       0x10u
#define PIC32MK_I2CxADD        0x20u
#define PIC32MK_I2CxMSK        0x30u
#define PIC32MK_I2CxTRN        0x40u
#define PIC32MK_I2CxRCV        0x50u

/* -----------------------------------------------------------------------
 * GPIO peripheral registers (§12, DS60001519E)
 * Base: PIC32MK_GPIO_OFFSET = 0x060000 (0xBF860000)
 * Each port (A–G) occupies 0x100 bytes.
 * ----------------------------------------------------------------------- */

#define PIC32MK_GPIO_PORT_SIZE  0x100u      /* per-port register block */

/* GPIO register offsets within each port block */
#define PIC32MK_ANSEL           0x00u   /* Analog select */
#define PIC32MK_TRIS            0x10u   /* Direction (1=input) */
#define PIC32MK_PORT            0x20u   /* Read pin state */
#define PIC32MK_LAT             0x30u   /* Latch (write output) */
#define PIC32MK_ODC             0x40u   /* Open-drain control */
#define PIC32MK_CNPU            0x50u   /* Change-notice pull-up */
#define PIC32MK_CNPD            0x60u   /* Change-notice pull-down */
#define PIC32MK_CNCON           0x70u   /* Change-notice control */
#define PIC32MK_CNEN0           0x80u   /* CN edge enable 0 */
#define PIC32MK_CNSTAT          0x90u   /* CN status */
#define PIC32MK_CNEN1           0xA0u   /* CN edge enable 1 */
#define PIC32MK_CNF             0xB0u   /* CN flag */

/* Number of GPIO ports (A–G) */
#define PIC32MK_GPIO_NPORTS     7

/* -----------------------------------------------------------------------
 * DMA controller registers (§26, DS60001519E)
 * Base: PIC32MK_DMA_OFFSET = 0x011000 (within EVIC page, 0xBF811000)
 * Global registers at base, then 8 channel blocks at +0x60 each.
 * ----------------------------------------------------------------------- */

/* DMA global registers */
#define PIC32MK_DMACON_OFFSET   0x00u   /* DMA control */
#define PIC32MK_DMASTAT_OFFSET  0x10u   /* DMA status */
#define PIC32MK_DMAADDR_OFFSET  0x20u   /* DMA address */

/* DMA channel registers base: +0x60 + channel * 0xC0 */
#define PIC32MK_DMA_CH_BASE     0x60u
#define PIC32MK_DMA_CH_STRIDE   0xC0u
#define PIC32MK_DMA_NCHANNELS   8

/* DMA channel register offsets within each channel block */
#define PIC32MK_DCHxCON         0x00u
#define PIC32MK_DCHxECON        0x10u
#define PIC32MK_DCHxINT         0x20u
#define PIC32MK_DCHxSSA         0x30u
#define PIC32MK_DCHxDSA         0x40u
#define PIC32MK_DCHxSSIZ        0x50u
#define PIC32MK_DCHxDSIZ        0x60u
#define PIC32MK_DCHxSPTR        0x70u
#define PIC32MK_DCHxDPTR        0x80u
#define PIC32MK_DCHxCSIZ        0x90u
#define PIC32MK_DCHxCPTR        0xA0u
#define PIC32MK_DCHxDAT         0xB0u

#endif /* HW_MIPS_PIC32MK_H */
