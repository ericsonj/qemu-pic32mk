/*
 * xc.h — GCC/mipsel compatibility shim for Microchip XC32 built-ins.
 *
 * Provides CP0 access macros and PIC32MK SFR register definitions so that
 * the FreeRTOS PIC32MK port (port.c / portmacro.h / port_asm.S) compiles
 * with mipsel-linux-gnu-gcc.
 *
 * SFR base addresses come from pic32mk.h constants:
 *   EVIC     0xBF810000  (SFR_BASE + EVIC_OFFSET = 0xBF800000 + 0x010000)
 *   Timer1   0xBF820000  (SFR_BASE + T1_OFFSET   = 0xBF800000 + 0x020000)
 *   UART1    0xBF828000  (SFR_BASE + UART1_OFFSET = 0xBF800000 + 0x028000)
 */

#ifndef XC_H
#define XC_H

/* ========================================================================
 * Assembly context — define CP0 register names as GAS operand tokens
 * ======================================================================== */
#ifdef __ASSEMBLER__

/* Register name aliases (zero, k0, k1, sp, gp, ra, s0-s7, t0-t9, etc.)
 * GNU AS for MIPS requires the $ prefix; sys/regdef.h provides CPP macros
 * that expand bare names (e.g. k0) to their $-prefixed form ($26). */
#include <sys/regdef.h>

#define _CP0_STATUS     $12, 0
#define _CP0_CAUSE      $13, 0
#define _CP0_EPC        $14, 0
#define _CP0_COUNT      $9,  0
#define _CP0_COMPARE    $11, 0
#define _CP0_EBASE      $15, 1
#define _CP0_INTCTL     $12, 1
#define _CP0_SRSCTL     $12, 2

/* SFR addresses used directly in assembly (e.g. "la reg, IFS0CLR")
 * PIC32MK: CLR=+4, SET=+8 from each register base */
#define IFS0CLR         0xBF810044
#define IEC0SET         0xBF8100C8
#define IEC0CLR         0xBF8100C4

#else /* C context */

#include <stdint.h>

/* -----------------------------------------------------------------------
 * CP0 access macros using GCC inline asm
 * ----------------------------------------------------------------------- */

#define _CP0_GET_STATUS() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$12,0" : "=r"(__r)); __r; })

#define _CP0_SET_STATUS(_v) \
    __asm__ volatile("mtc0 %0,$12,0; ehb" : : "r"((uint32_t)(_v)))

#define _CP0_GET_CAUSE() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$13,0" : "=r"(__r)); __r; })

#define _CP0_SET_CAUSE(_v) \
    __asm__ volatile("mtc0 %0,$13,0; ehb" : : "r"((uint32_t)(_v)))

#define _CP0_GET_EPC() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$14,0" : "=r"(__r)); __r; })

#define _CP0_GET_COUNT() \
    ({ uint32_t __r; __asm__ volatile("mfc0 %0,$9,0" : "=r"(__r)); __r; })

#define _CP0_SET_COMPARE(_v) \
    __asm__ volatile("mtc0 %0,$11,0; ehb" : : "r"((uint32_t)(_v)))

/* CLZ — count leading zeros */
static inline unsigned int _clz(unsigned int x)
{
    return x ? __builtin_clz(x) : 32u;
}

/* Bit-set / bit-clear helpers for CP0 registers (XC32 built-ins) */
#define _CP0_BIS_STATUS(_mask) \
    _CP0_SET_STATUS(_CP0_GET_STATUS() | (uint32_t)(_mask))

#define _CP0_BIC_STATUS(_mask) \
    _CP0_SET_STATUS(_CP0_GET_STATUS() & ~(uint32_t)(_mask))

#define _CP0_BIS_CAUSE(_mask) \
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() | (uint32_t)(_mask))

#define _CP0_BIC_CAUSE(_mask) \
    _CP0_SET_CAUSE(_CP0_GET_CAUSE() & ~(uint32_t)(_mask))

/* XC32 built-in to disable interrupts — GCC equivalent of 'di' */
static inline void __builtin_disable_interrupts(void)
{
    __asm__ volatile("di; ehb" : : : "memory");
}

/* XC32 built-in to enable interrupts */
static inline void __builtin_enable_interrupts(void)
{
    __asm__ volatile("ei; ehb" : : : "memory");
}

/* -----------------------------------------------------------------------
 * EVIC registers  (base 0xBF810000)
 * IFS/IEC/IPC each have SET (+4), CLR (+8), INV (+C) sub-registers.
 * ----------------------------------------------------------------------- */

#define _EVIC_BASE      0xBF810000u

/* EVIC CLR/SET/INV convention (PIC32MK): CLR at +4, SET at +8, INV at +C */
#define IFS0            (*(volatile uint32_t *)(_EVIC_BASE + 0x0040u))
#define IFS0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0044u))
#define IFS0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0048u))
#define IFS0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x004Cu))

#define IFS1            (*(volatile uint32_t *)(_EVIC_BASE + 0x0050u))
#define IFS1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0054u))
#define IFS1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0058u))
#define IFS1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x005Cu))

#define IEC0            (*(volatile uint32_t *)(_EVIC_BASE + 0x00C0u))
#define IEC0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00C4u))
#define IEC0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00C8u))
#define IEC0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00CCu))

#define IEC1            (*(volatile uint32_t *)(_EVIC_BASE + 0x00D0u))
#define IEC1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00D4u))
#define IEC1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00D8u))
#define IEC1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00DCu))

/* UART1 interrupt bit positions in IFS1/IEC1 (from p32mk1024mcm100.h):
 *   U1E = bit 6, U1RX = bit 7, U1TX = bit 8 */
#define _IFS1_U1EIF_MASK        0x00000040u
#define _IFS1_U1RXIF_MASK       0x00000080u
#define _IFS1_U1TXIF_MASK       0x00000100u
#define _IEC1_U1EIE_MASK        0x00000040u
#define _IEC1_U1RXIE_MASK       0x00000080u
#define _IEC1_U1TXIE_MASK       0x00000100u

#define IPC0            (*(volatile uint32_t *)(_EVIC_BASE + 0x0140u))
#define IPC0CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0144u))
#define IPC0SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0148u))
#define IPC0INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x014Cu))

/* IPC1 covers EVIC sources 4-7.  Timer1 is source 4 → byte 0 of IPC1.
 * IP field at bits [4:2], IS field at bits [1:0]. */
#define IPC1            (*(volatile uint32_t *)(_EVIC_BASE + 0x0150u))
#define IPC1CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0154u))
#define IPC1SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0158u))
#define IPC1INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x015Cu))

/* IPC1 bit-field struct for T1IP / T1IS access */
typedef union {
    struct {
        uint32_t T1IS   : 2;   /* bits [1:0]  subpriority for Timer1 */
        uint32_t T1IP   : 3;   /* bits [4:2]  priority    for Timer1 */
        uint32_t        : 27;
    };
    uint32_t w;
} __IPC1_T1_t;

#define IPC1bits        (*(volatile __IPC1_T1_t *)(_EVIC_BASE + 0x0150u))

/*
 * IPC8 covers EVIC sources 32-35.  USB1 = source 34 → byte 2 of IPC8.
 * IPC8 offset = 0x0140 + 8*0x10 = 0x01C0.
 * USB1 priority bits = [18:16], subpriority [20:19].
 */
#define IPC8            (*(volatile uint32_t *)(_EVIC_BASE + 0x01C0u))
#define IPC8CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01C4u))
#define IPC8SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01C8u))
#define IPC8INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01CCu))
#define _IPC8_USB1IP_POSITION   18u   /* USB1 priority bits [20:18] (EVIC shift+2) */
#define _IPC8_USB1IP_MASK       (0x7u << 18u)
#define _IPC8_USB1IS_POSITION   16u   /* USB1 subpriority bits [17:16] */
#define _IPC8_USB1IS_MASK       (0x3u << 16u)

/*
 * IPC9 covers EVIC sources 36-39.  UART1 FAULT=38 (byte 2), RX=39 (byte 3).
 * IPC9 offset = 0x0140 + 9*0x10 = 0x01D0.
 */
#define IPC9            (*(volatile uint32_t *)(_EVIC_BASE + 0x01D0u))
#define IPC9CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x01D4u))
#define IPC9SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x01D8u))
#define IPC9INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x01DCu))

/*
 * IPC10 covers EVIC sources 40-43.  UART1 TX=40 → byte 0 of IPC10.
 * IPC10 offset = 0x0140 + 10*0x10 = 0x01E0.
 */
#define IPC10           (*(volatile uint32_t *)(_EVIC_BASE + 0x01E0u))
#define IPC10CLR        (*(volatile uint32_t *)(_EVIC_BASE + 0x01E4u))
#define IPC10SET        (*(volatile uint32_t *)(_EVIC_BASE + 0x01E8u))
#define IPC10INV        (*(volatile uint32_t *)(_EVIC_BASE + 0x01ECu))

/* IFS0 / IEC0 named bit masks (source number = bit position) */
#define _IFS0_T1IF_MASK         (1u << 4)   /* Timer1 = EVIC source 4 */
#define _IFS0_CS0IF_MASK        (1u << 1)   /* CoreSW0 = EVIC source 1 */
#define _IEC0_CS0IE_MASK        (1u << 1)
#define _IEC0_CS0IE_POSITION    1
#define _IPC0_CS0IP_MASK        (0x7u << 10)
#define _IPC0_CS0IP_POSITION    10

/* -----------------------------------------------------------------------
 * Timer1 registers  (base 0xBF820000, stride 0x10)
 * ----------------------------------------------------------------------- */

#define _T1_BASE        0xBF820000u

#define T1CON           (*(volatile uint32_t *)(_T1_BASE + 0x000u))
#define T1CONCLR        (*(volatile uint32_t *)(_T1_BASE + 0x004u))
#define T1CONSET        (*(volatile uint32_t *)(_T1_BASE + 0x008u))
#define TMR1            (*(volatile uint32_t *)(_T1_BASE + 0x010u))
#define PR1             (*(volatile uint32_t *)(_T1_BASE + 0x020u))

/* T1CON bit-field struct */
typedef union {
    struct {
        uint32_t            : 1;   /* bit 0  reserved */
        uint32_t TCS        : 1;   /* bit 1  clock source */
        uint32_t TSYNC      : 1;   /* bit 2  sync */
        uint32_t            : 1;   /* bit 3  reserved */
        uint32_t TCKPS      : 2;   /* bits [5:4]  prescaler: 0=1:1,1=1:8,2=1:64,3=1:256 */
        uint32_t            : 1;   /* bit 6  reserved */
        uint32_t TGATE      : 1;   /* bit 7  gated accumulation */
        uint32_t            : 5;   /* bits [12:8] reserved */
        uint32_t SIDL       : 1;   /* bit 13 stop-in-idle */
        uint32_t            : 1;   /* bit 14 reserved */
        uint32_t ON         : 1;   /* bit 15 timer on — Microchip also calls this TON */
        uint32_t            : 16;
    };
    /* Alias so port.c's T1CONbits.TON works too */
    struct {
        uint32_t            : 15;
        uint32_t TON        : 1;
        uint32_t            : 16;
    };
    uint32_t w;
} __T1CON_t;

#define T1CONbits       (*(volatile __T1CON_t *)(_T1_BASE + 0x000u))

/* IFS0 bit-field struct (32 sources, one bit each) */
typedef union {
    struct {
        uint32_t CTIF   : 1;   /* bit 0: Core Timer */
        uint32_t CS0IF  : 1;   /* bit 1: Core SW0 */
        uint32_t CS1IF  : 1;   /* bit 2: Core SW1 */
        uint32_t INT0IF : 1;   /* bit 3: External INT0 */
        uint32_t T1IF   : 1;   /* bit 4: Timer1 */
        uint32_t        : 27;
    };
    uint32_t w;
} __IFS0_t;

#define IFS0bits        (*(volatile __IFS0_t *)(_EVIC_BASE + 0x0040u))

/* IEC0 bit-field struct */
typedef union {
    struct {
        uint32_t CTIE   : 1;   /* bit 0 */
        uint32_t CS0IE  : 1;   /* bit 1 */
        uint32_t CS1IE  : 1;   /* bit 2 */
        uint32_t INT0IE : 1;   /* bit 3 */
        uint32_t T1IE   : 1;   /* bit 4 */
        uint32_t        : 27;
    };
    uint32_t w;
} __IEC0_t;

#define IEC0bits        (*(volatile __IEC0_t *)(_EVIC_BASE + 0x00C0u))

/* -----------------------------------------------------------------------
 * UART1 registers  (base 0xBF828000)
 * ----------------------------------------------------------------------- */

#define _U1_BASE        0xBF828000u

#define U1MODE          (*(volatile uint32_t *)(_U1_BASE + 0x000u))
#define U1MODECLR       (*(volatile uint32_t *)(_U1_BASE + 0x004u))
#define U1MODESET       (*(volatile uint32_t *)(_U1_BASE + 0x008u))
#define U1MODEINV       (*(volatile uint32_t *)(_U1_BASE + 0x00Cu))
#define U1STA           (*(volatile uint32_t *)(_U1_BASE + 0x010u))
#define U1STACLR        (*(volatile uint32_t *)(_U1_BASE + 0x014u))
#define U1STASET        (*(volatile uint32_t *)(_U1_BASE + 0x018u))
#define U1STAINV        (*(volatile uint32_t *)(_U1_BASE + 0x01Cu))
#define U1TXREG         (*(volatile uint32_t *)(_U1_BASE + 0x020u))
#define U1RXREG         (*(volatile uint32_t *)(_U1_BASE + 0x030u))
#define U1BRG           (*(volatile uint32_t *)(_U1_BASE + 0x040u))
#define U1BRGCLR        (*(volatile uint32_t *)(_U1_BASE + 0x044u))
#define U1BRGSET        (*(volatile uint32_t *)(_U1_BASE + 0x048u))
#define U1BRGINV        (*(volatile uint32_t *)(_U1_BASE + 0x04Cu))

/* U1MODE bit masks (plib-compatible _MASK suffix) */
#define _U1MODE_STSEL_MASK      0x00000001u
#define _U1MODE_PDSEL0_MASK     0x00000002u
#define _U1MODE_PDSEL1_MASK     0x00000004u
#define _U1MODE_PDSEL_MASK      0x00000006u
#define _U1MODE_BRGH_MASK       0x00000008u
#define _U1MODE_RXINV_MASK      0x00000010u
#define _U1MODE_ABAUD_MASK      0x00000020u
#define _U1MODE_LPBACK_MASK     0x00000040u
#define _U1MODE_WAKE_MASK       0x00000080u
#define _U1MODE_SIDL_MASK       0x00002000u
#define _U1MODE_ON_MASK         0x00008000u

/* U1STA bit masks (plib-compatible _MASK suffix) */
#define _U1STA_URXDA_MASK       0x00000001u
#define _U1STA_OERR_MASK        0x00000002u
#define _U1STA_FERR_MASK        0x00000004u
#define _U1STA_PERR_MASK        0x00000008u
#define _U1STA_RIDLE_MASK       0x00000010u
#define _U1STA_ADDEN_MASK       0x00000020u
#define _U1STA_TRMT_MASK        0x00000100u
#define _U1STA_UTXBF_MASK       0x00000200u
#define _U1STA_UTXEN_MASK       0x00000400u
#define _U1STA_UTXBRK_MASK      0x00000800u
#define _U1STA_URXEN_MASK       0x00001000u
#define _U1STA_UTXISEL0_MASK    0x00004000u
#define _U1STA_UTXISEL1_MASK    0x00008000u

/* Legacy shorthand (backward compat) */
#define _U1STA_UTXBF    _U1STA_UTXBF_MASK
#define _U1STA_UTXEN    _U1STA_UTXEN_MASK
#define _U1MODE_ON      _U1MODE_ON_MASK

/* -----------------------------------------------------------------------
 * IFS2 / IEC2  (sources 64–95)
 * ----------------------------------------------------------------------- */

#define IFS2            (*(volatile uint32_t *)(_EVIC_BASE + 0x0060u))
#define IFS2CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0064u))
#define IFS2SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0068u))
#define IFS2INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x006Cu))

#define IEC2            (*(volatile uint32_t *)(_EVIC_BASE + 0x00E0u))
#define IEC2CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00E4u))
#define IEC2SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00E8u))
#define IEC2INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00ECu))

/* -----------------------------------------------------------------------
 * IFS3 / IEC3  (sources 96–127)
 * UART2: U2E=115 (bit 19), U2RX=116 (bit 20), U2TX=117 (bit 21)
 * ----------------------------------------------------------------------- */

#define IFS3            (*(volatile uint32_t *)(_EVIC_BASE + 0x0070u))
#define IFS3CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0074u))
#define IFS3SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0078u))
#define IFS3INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x007Cu))

#define IEC3            (*(volatile uint32_t *)(_EVIC_BASE + 0x00F0u))
#define IEC3CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00F4u))
#define IEC3SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00F8u))
#define IEC3INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00FCu))

#define _IFS3_U2EIF_MASK        (1u << 19)   /* source 115 */
#define _IFS3_U2RXIF_MASK       (1u << 20)   /* source 116 */
#define _IFS3_U2TXIF_MASK       (1u << 21)   /* source 117 */
#define _IEC3_U2EIE_MASK        (1u << 19)
#define _IEC3_U2RXIE_MASK       (1u << 20)
#define _IEC3_U2TXIE_MASK       (1u << 21)

/* -----------------------------------------------------------------------
 * UART2 registers  (base 0xBF828200)
 * ----------------------------------------------------------------------- */

#define _U2_BASE        0xBF828200u

#define U2MODE          (*(volatile uint32_t *)(_U2_BASE + 0x000u))
#define U2MODECLR       (*(volatile uint32_t *)(_U2_BASE + 0x004u))
#define U2MODESET       (*(volatile uint32_t *)(_U2_BASE + 0x008u))
#define U2MODEINV       (*(volatile uint32_t *)(_U2_BASE + 0x00Cu))
#define U2STA           (*(volatile uint32_t *)(_U2_BASE + 0x010u))
#define U2STACLR        (*(volatile uint32_t *)(_U2_BASE + 0x014u))
#define U2STASET        (*(volatile uint32_t *)(_U2_BASE + 0x018u))
#define U2STAINV        (*(volatile uint32_t *)(_U2_BASE + 0x01Cu))
#define U2TXREG         (*(volatile uint32_t *)(_U2_BASE + 0x020u))
#define U2RXREG         (*(volatile uint32_t *)(_U2_BASE + 0x030u))
#define U2BRG           (*(volatile uint32_t *)(_U2_BASE + 0x040u))
#define U2BRGCLR        (*(volatile uint32_t *)(_U2_BASE + 0x044u))
#define U2BRGSET        (*(volatile uint32_t *)(_U2_BASE + 0x048u))
#define U2BRGINV        (*(volatile uint32_t *)(_U2_BASE + 0x04Cu))

/* U2MODE bit masks (same field layout as U1MODE) */
#define _U2MODE_STSEL_MASK      0x00000001u
#define _U2MODE_PDSEL0_MASK     0x00000002u
#define _U2MODE_PDSEL1_MASK     0x00000004u
#define _U2MODE_PDSEL_MASK      0x00000006u
#define _U2MODE_BRGH_MASK       0x00000008u
#define _U2MODE_RXINV_MASK      0x00000010u
#define _U2MODE_ABAUD_MASK      0x00000020u
#define _U2MODE_LPBACK_MASK     0x00000040u
#define _U2MODE_WAKE_MASK       0x00000080u
#define _U2MODE_SIDL_MASK       0x00002000u
#define _U2MODE_ON_MASK         0x00008000u

/* U2STA bit masks (same field layout as U1STA) */
#define _U2STA_URXDA_MASK       0x00000001u
#define _U2STA_OERR_MASK        0x00000002u
#define _U2STA_FERR_MASK        0x00000004u
#define _U2STA_PERR_MASK        0x00000008u
#define _U2STA_RIDLE_MASK       0x00000010u
#define _U2STA_ADDEN_MASK       0x00000020u
#define _U2STA_TRMT_MASK        0x00000100u
#define _U2STA_UTXBF_MASK       0x00000200u
#define _U2STA_UTXEN_MASK       0x00000400u
#define _U2STA_UTXBRK_MASK      0x00000800u
#define _U2STA_URXEN_MASK       0x00001000u
#define _U2STA_UTXISEL0_MASK    0x00004000u
#define _U2STA_UTXISEL1_MASK    0x00008000u

/* -----------------------------------------------------------------------
 * IFS4 / IFS5  (sources 128–159, 160–191)
 * IEC4 / IEC5
 * CAN1 combined interrupt = EVIC source 167 → IFS5/IEC5 bit 7
 * ----------------------------------------------------------------------- */

#define IFS4            (*(volatile uint32_t *)(_EVIC_BASE + 0x0080u))
#define IFS4CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0084u))
#define IFS4SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0088u))
#define IFS4INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x008Cu))

#define IFS5            (*(volatile uint32_t *)(_EVIC_BASE + 0x0090u))
#define IFS5CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0094u))
#define IFS5SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0098u))
#define IFS5INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x009Cu))

#define IEC4            (*(volatile uint32_t *)(_EVIC_BASE + 0x0100u))
#define IEC4CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0104u))
#define IEC4SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0108u))
#define IEC4INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x010Cu))

#define IEC5            (*(volatile uint32_t *)(_EVIC_BASE + 0x0110u))
#define IEC5CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0114u))
#define IEC5SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0118u))
#define IEC5INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x011Cu))

/* CAN1 = EVIC source 167 = IFS5/IEC5 bit 7 */
#define _IFS5_CAN1IF_MASK   0x00000080u
#define _IEC5_CAN1IE_MASK   0x00000080u

/* -----------------------------------------------------------------------
 * CAN1 (CFD1*) SFR registers — base 0xBF880000
 * Layout verified against XC32 p32mk1024mcm100.h device header.
 * Each register block: +0 base, +4 SET, +8 CLR, +C INV
 * ----------------------------------------------------------------------- */

#define _CFD1_BASE      0xBF880000u

/* CiCON */
#define CFD1CON         (*(volatile uint32_t *)(_CFD1_BASE + 0x000u))
#define CFD1CONSET      (*(volatile uint32_t *)(_CFD1_BASE + 0x004u))
#define CFD1CONCLR      (*(volatile uint32_t *)(_CFD1_BASE + 0x008u))
#define CFD1CONINV      (*(volatile uint32_t *)(_CFD1_BASE + 0x00Cu))

#define _CFD1CON_CLKSEL0_POSITION   7
#define _CFD1CON_CLKSEL0_MASK       0x00000080u
#define _CFD1CON_ON_MASK            0x00008000u
#define _CFD1CON_STEF_POSITION      19
#define _CFD1CON_STEF_MASK          0x00080000u
#define _CFD1CON_TXQEN_POSITION     20
#define _CFD1CON_TXQEN_MASK         0x00100000u
#define _CFD1CON_OPMOD_POSITION     21
#define _CFD1CON_OPMOD_MASK         0x00E00000u
#define _CFD1CON_REQOP_POSITION     24
#define _CFD1CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD1NBTCFG      (*(volatile uint32_t *)(_CFD1_BASE + 0x010u))

#define _CFD1NBTCFG_SJW_POSITION    0
#define _CFD1NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD1NBTCFG_TSEG2_POSITION  8
#define _CFD1NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD1NBTCFG_TSEG1_POSITION  16
#define _CFD1NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD1NBTCFG_BRP_POSITION    24
#define _CFD1NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD1DBTCFG      (*(volatile uint32_t *)(_CFD1_BASE + 0x020u))

#define _CFD1DBTCFG_SJW_POSITION    0
#define _CFD1DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD1DBTCFG_TSEG2_POSITION  8
#define _CFD1DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD1DBTCFG_TSEG1_POSITION  16
#define _CFD1DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD1DBTCFG_BRP_POSITION    24
#define _CFD1DBTCFG_BRP_MASK        0xFF000000u

/* CiVEC */
#define CFD1VEC         (*(volatile uint32_t *)(_CFD1_BASE + 0x060u))
#define _CFD1VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD1INT         (*(volatile uint32_t *)(_CFD1_BASE + 0x070u))
#define CFD1INTSET      (*(volatile uint32_t *)(_CFD1_BASE + 0x074u))
#define CFD1INTCLR      (*(volatile uint32_t *)(_CFD1_BASE + 0x078u))

/* CiINT status flags (lower 16 bits) */
#define _CFD1INT_TXIF_MASK          0x00000001u   /* bit 0 */
#define _CFD1INT_RXIF_MASK          0x00000002u   /* bit 1 */
#define _CFD1INT_SERRIF_MASK        0x00001000u   /* bit 12 */
#define _CFD1INT_CERRIF_MASK        0x00002000u   /* bit 13 */
#define _CFD1INT_IVMIF_MASK         0x00008000u   /* bit 15 */
/* CiINT interrupt enables (upper 16 bits) */
#define _CFD1INT_TXIE_MASK          0x00010000u   /* bit 16 */
#define _CFD1INT_RXIE_MASK          0x00020000u   /* bit 17 */
#define _CFD1INT_SERRIE_MASK        0x10000000u   /* bit 28 */
#define _CFD1INT_CERRIE_MASK        0x20000000u   /* bit 29 */
#define _CFD1INT_IVMIE_MASK         0x80000000u   /* bit 31 */

/* CiTREC */
#define CFD1TREC        (*(volatile uint32_t *)(_CFD1_BASE + 0x0D0u))

#define _CFD1TREC_TERRCNT_POSITION  0
#define _CFD1TREC_TERRCNT_MASK      0x000000FFu
#define _CFD1TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD1TREC_EWARN_MASK        0x00010000u
#define _CFD1TREC_RXWARN_MASK       0x00020000u
#define _CFD1TREC_TXWARN_MASK       0x00040000u
#define _CFD1TREC_RXBP_MASK         0x00080000u
#define _CFD1TREC_TXBP_MASK         0x00100000u
#define _CFD1TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD1TEFCON      (*(volatile uint32_t *)(_CFD1_BASE + 0x100u))

#define _CFD1TEFCON_UINC_MASK       0x00000100u   /* bit 8 */
#define _CFD1TEFCON_FSIZE_POSITION  24
#define _CFD1TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD1TEFSTA      (*(volatile uint32_t *)(_CFD1_BASE + 0x110u))
#define _CFD1TEFSTA_TEFNEIF_MASK    0x00000001u   /* TEF Not Empty flag, bit 0 */

/* CiTEFUA */
#define CFD1TEFUA       (*(volatile uint32_t *)(_CFD1_BASE + 0x120u))

/* CiFIFOBA — message RAM base address (Phase 3C stub in emulator) */
#define CFD1FIFOBA      (*(volatile uint32_t *)(_CFD1_BASE + 0x130u))

/* CiTXQCON */
#define CFD1TXQCON      (*(volatile uint32_t *)(_CFD1_BASE + 0x140u))
#define CFD1TXQCONSET   (*(volatile uint32_t *)(_CFD1_BASE + 0x144u))
#define CFD1TXQCONCLR   (*(volatile uint32_t *)(_CFD1_BASE + 0x148u))

#define _CFD1TXQCON_TXQEIE_MASK     0x00000010u   /* TX Queue Empty IE, bit 4 */
#define _CFD1TXQCON_UINC_MASK       0x00000100u   /* User Increment, bit 8 */
#define _CFD1TXQCON_TXREQ_MASK      0x00000200u   /* TX Request, bit 9 */
#define _CFD1TXQCON_TXPRI_POSITION  16
#define _CFD1TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD1TXQCON_FSIZE_POSITION  24
#define _CFD1TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD1TXQCON_PLSIZE_POSITION 29
#define _CFD1TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD1TXQSTA      (*(volatile uint32_t *)(_CFD1_BASE + 0x150u))
#define _CFD1TXQSTA_TXQNIF_MASK     0x00000001u   /* TX Queue Not Full, bit 0 */

/* CiTXQUA */
#define CFD1TXQUA       (*(volatile uint32_t *)(_CFD1_BASE + 0x160u))

/* CiFIFOCON1 / CiFIFOCON2 (FIFO 2 = FIFO 1 base + 0x30) */
#define CFD1FIFOCON1    (*(volatile uint32_t *)(_CFD1_BASE + 0x170u))
#define CFD1FIFOCON1SET (*(volatile uint32_t *)(_CFD1_BASE + 0x174u))
#define CFD1FIFOCON1CLR (*(volatile uint32_t *)(_CFD1_BASE + 0x178u))

#define _CFD1FIFOCON1_TFNRFNIE_MASK  0x00000001u  /* Not Empty IE, bit 0 */
#define _CFD1FIFOCON1_TFERFFIE_MASK  0x00000010u  /* TX Empty IE, bit 4 */
#define _CFD1FIFOCON1_RTREN_POSITION 6
#define _CFD1FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD1FIFOCON1_TXEN_MASK      0x00000080u  /* TX enable, bit 7 */
#define _CFD1FIFOCON1_UINC_MASK      0x00000100u  /* User Increment, bit 8 */
#define _CFD1FIFOCON1_TXREQ_MASK     0x00000200u  /* TX Request, bit 9 */
#define _CFD1FIFOCON1_TXPRI_POSITION 16
#define _CFD1FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD1FIFOCON1_FSIZE_POSITION 24
#define _CFD1FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD1FIFOCON1_PLSIZE_POSITION 29
#define _CFD1FIFOCON1_PLSIZE_MASK    0xE0000000u

#define CFD1FIFOCON2    (*(volatile uint32_t *)(_CFD1_BASE + 0x1A0u))

#define _CFD1FIFOCON2_FSIZE_POSITION 24
#define _CFD1FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD1FIFOCON2_PLSIZE_POSITION 29
#define _CFD1FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 (FIFOSTA2 = FIFOSTA1 + 0x30 via pointer arithmetic in plib) */
#define CFD1FIFOSTA1    (*(volatile uint32_t *)(_CFD1_BASE + 0x180u))
#define _CFD1FIFOSTA1_TFNRFNIF_MASK  0x00000001u  /* Not Empty flag, bit 0 */
#define _CFD1FIFOSTA1_TXATIF_MASK    0x00000010u  /* TX Attempts exhausted, bit 4 */

/* CiFIFOUA1 */
#define CFD1FIFOUA1     (*(volatile uint32_t *)(_CFD1_BASE + 0x190u))

/* CiFLTCON0 */
#define CFD1FLTCON0     (*(volatile uint32_t *)(_CFD1_BASE + 0x740u))
#define CFD1FLTCON0SET  (*(volatile uint32_t *)(_CFD1_BASE + 0x744u))

#define _CFD1FLTCON0_F0BP_POSITION   0
#define _CFD1FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD1FLTCON0_FLTEN0_POSITION 7
#define _CFD1FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 (stride 0x20 per pair via pointer arithmetic in plib) */
#define CFD1FLTOBJ0     (*(volatile uint32_t *)(_CFD1_BASE + 0x7C0u))
#define _CFD1FLTOBJ0_EXIDE_MASK      0x20000000u  /* Extended ID enable, bit 29 */

/* CiMASK0 */
#define CFD1MASK0       (*(volatile uint32_t *)(_CFD1_BASE + 0x7D0u))
#define _CFD1MASK0_MIDE_MASK         0x20000000u  /* Match IDE bit, bit 29 */

/* -----------------------------------------------------------------------
 * CAN2 = EVIC source 168 = IFS5/IEC5 bit 8
 * ----------------------------------------------------------------------- */
#define _IFS5_CAN2IF_MASK   0x00000100u
#define _IEC5_CAN2IE_MASK   0x00000100u

/* -----------------------------------------------------------------------
 * CAN2 (CFD2*) SFR registers — base 0xBF881000
 * Same layout as CFD1; only the base address differs.
 * ----------------------------------------------------------------------- */

#define _CFD2_BASE      0xBF881000u

/* CiCON */
#define CFD2CON         (*(volatile uint32_t *)(_CFD2_BASE + 0x000u))
#define CFD2CONSET      (*(volatile uint32_t *)(_CFD2_BASE + 0x004u))
#define CFD2CONCLR      (*(volatile uint32_t *)(_CFD2_BASE + 0x008u))
#define CFD2CONINV      (*(volatile uint32_t *)(_CFD2_BASE + 0x00Cu))

#define _CFD2CON_CLKSEL0_POSITION   7
#define _CFD2CON_CLKSEL0_MASK       0x00000080u
#define _CFD2CON_ON_MASK            0x00008000u
#define _CFD2CON_STEF_POSITION      19
#define _CFD2CON_STEF_MASK          0x00080000u
#define _CFD2CON_TXQEN_POSITION     20
#define _CFD2CON_TXQEN_MASK         0x00100000u
#define _CFD2CON_OPMOD_POSITION     21
#define _CFD2CON_OPMOD_MASK         0x00E00000u
#define _CFD2CON_REQOP_POSITION     24
#define _CFD2CON_REQOP_MASK         0x07000000u

/* CiNBTCFG */
#define CFD2NBTCFG      (*(volatile uint32_t *)(_CFD2_BASE + 0x010u))

#define _CFD2NBTCFG_SJW_POSITION    0
#define _CFD2NBTCFG_SJW_MASK        0x0000007Fu
#define _CFD2NBTCFG_TSEG2_POSITION  8
#define _CFD2NBTCFG_TSEG2_MASK      0x00007F00u
#define _CFD2NBTCFG_TSEG1_POSITION  16
#define _CFD2NBTCFG_TSEG1_MASK      0x00FF0000u
#define _CFD2NBTCFG_BRP_POSITION    24
#define _CFD2NBTCFG_BRP_MASK        0xFF000000u

/* CiDBTCFG */
#define CFD2DBTCFG      (*(volatile uint32_t *)(_CFD2_BASE + 0x020u))

#define _CFD2DBTCFG_SJW_POSITION    0
#define _CFD2DBTCFG_SJW_MASK        0x0000000Fu
#define _CFD2DBTCFG_TSEG2_POSITION  8
#define _CFD2DBTCFG_TSEG2_MASK      0x00000F00u
#define _CFD2DBTCFG_TSEG1_POSITION  16
#define _CFD2DBTCFG_TSEG1_MASK      0x001F0000u
#define _CFD2DBTCFG_BRP_POSITION    24
#define _CFD2DBTCFG_BRP_MASK        0xFF000000u

/* CiTBC */
#define CFD2TBC         (*(volatile uint32_t *)(_CFD2_BASE + 0x040u))

/* CiVEC */
#define CFD2VEC         (*(volatile uint32_t *)(_CFD2_BASE + 0x060u))
#define _CFD2VEC_ICODE_MASK         0x0000007Fu

/* CiINT */
#define CFD2INT         (*(volatile uint32_t *)(_CFD2_BASE + 0x070u))
#define CFD2INTSET      (*(volatile uint32_t *)(_CFD2_BASE + 0x074u))
#define CFD2INTCLR      (*(volatile uint32_t *)(_CFD2_BASE + 0x078u))

#define _CFD2INT_TXIF_MASK          0x00000001u
#define _CFD2INT_RXIF_MASK          0x00000002u
#define _CFD2INT_MODIF_MASK         0x00000008u
#define _CFD2INT_SERRIF_MASK        0x00001000u
#define _CFD2INT_CERRIF_MASK        0x00002000u
#define _CFD2INT_IVMIF_MASK         0x00008000u
#define _CFD2INT_TXIE_MASK          0x00010000u
#define _CFD2INT_RXIE_MASK          0x00020000u
#define _CFD2INT_MODIE_MASK         0x00080000u
#define _CFD2INT_SERRIE_MASK        0x10000000u
#define _CFD2INT_CERRIE_MASK        0x20000000u
#define _CFD2INT_IVMIE_MASK         0x80000000u

/* CiRXIF / CiTXIF / CiRXOVIF / CiTXATIF */
#define CFD2RXIF        (*(volatile uint32_t *)(_CFD2_BASE + 0x080u))
#define CFD2TXIF        (*(volatile uint32_t *)(_CFD2_BASE + 0x090u))
#define CFD2RXOVIF      (*(volatile uint32_t *)(_CFD2_BASE + 0x0A0u))
#define CFD2TXATIF      (*(volatile uint32_t *)(_CFD2_BASE + 0x0B0u))

/* CiTREC */
#define CFD2TREC        (*(volatile uint32_t *)(_CFD2_BASE + 0x0D0u))

#define _CFD2TREC_TERRCNT_POSITION  0
#define _CFD2TREC_TERRCNT_MASK      0x000000FFu
#define _CFD2TREC_RERRCNT_MASK      0x0000FF00u
#define _CFD2TREC_EWARN_MASK        0x00010000u
#define _CFD2TREC_RXWARN_MASK       0x00020000u
#define _CFD2TREC_TXWARN_MASK       0x00040000u
#define _CFD2TREC_RXBP_MASK         0x00080000u
#define _CFD2TREC_TXBP_MASK         0x00100000u
#define _CFD2TREC_TXBO_MASK         0x00200000u

/* CiTEFCON */
#define CFD2TEFCON      (*(volatile uint32_t *)(_CFD2_BASE + 0x100u))
#define CFD2TEFCONSET   (*(volatile uint32_t *)(_CFD2_BASE + 0x104u))
#define CFD2TEFCONCLR   (*(volatile uint32_t *)(_CFD2_BASE + 0x108u))

#define _CFD2TEFCON_UINC_MASK       0x00000100u
#define _CFD2TEFCON_FSIZE_POSITION  24
#define _CFD2TEFCON_FSIZE_MASK      0x1F000000u

/* CiTEFSTA */
#define CFD2TEFSTA      (*(volatile uint32_t *)(_CFD2_BASE + 0x110u))
#define _CFD2TEFSTA_TEFNEIF_MASK    0x00000001u

/* CiTEFUA */
#define CFD2TEFUA       (*(volatile uint32_t *)(_CFD2_BASE + 0x120u))

/* CiFIFOBA */
#define CFD2FIFOBA      (*(volatile uint32_t *)(_CFD2_BASE + 0x130u))

/* CiTXQCON */
#define CFD2TXQCON      (*(volatile uint32_t *)(_CFD2_BASE + 0x140u))
#define CFD2TXQCONSET   (*(volatile uint32_t *)(_CFD2_BASE + 0x144u))
#define CFD2TXQCONCLR   (*(volatile uint32_t *)(_CFD2_BASE + 0x148u))

#define _CFD2TXQCON_TXQEIE_MASK     0x00000010u
#define _CFD2TXQCON_UINC_MASK       0x00000100u
#define _CFD2TXQCON_TXREQ_MASK      0x00000200u
#define _CFD2TXQCON_TXPRI_POSITION  16
#define _CFD2TXQCON_TXPRI_MASK      0x001F0000u
#define _CFD2TXQCON_FSIZE_POSITION  24
#define _CFD2TXQCON_FSIZE_MASK      0x1F000000u
#define _CFD2TXQCON_PLSIZE_POSITION 29
#define _CFD2TXQCON_PLSIZE_MASK     0xE0000000u

/* CiTXQSTA */
#define CFD2TXQSTA      (*(volatile uint32_t *)(_CFD2_BASE + 0x150u))
#define _CFD2TXQSTA_TXQNIF_MASK     0x00000001u

/* CiTXQUA */
#define CFD2TXQUA       (*(volatile uint32_t *)(_CFD2_BASE + 0x160u))

/* CiFIFOCON1 */
#define CFD2FIFOCON1    (*(volatile uint32_t *)(_CFD2_BASE + 0x170u))
#define CFD2FIFOCON1SET (*(volatile uint32_t *)(_CFD2_BASE + 0x174u))
#define CFD2FIFOCON1CLR (*(volatile uint32_t *)(_CFD2_BASE + 0x178u))

#define _CFD2FIFOCON1_TFNRFNIE_MASK  0x00000001u
#define _CFD2FIFOCON1_TFERFFIE_MASK  0x00000010u
#define _CFD2FIFOCON1_RTREN_POSITION 6
#define _CFD2FIFOCON1_RTREN_MASK     0x00000040u
#define _CFD2FIFOCON1_TXEN_MASK      0x00000080u
#define _CFD2FIFOCON1_UINC_MASK      0x00000100u
#define _CFD2FIFOCON1_TXREQ_MASK     0x00000200u
#define _CFD2FIFOCON1_TXPRI_POSITION 16
#define _CFD2FIFOCON1_TXPRI_MASK     0x001F0000u
#define _CFD2FIFOCON1_FSIZE_POSITION 24
#define _CFD2FIFOCON1_FSIZE_MASK     0x1F000000u
#define _CFD2FIFOCON1_PLSIZE_POSITION 29
#define _CFD2FIFOCON1_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA1 */
#define CFD2FIFOSTA1    (*(volatile uint32_t *)(_CFD2_BASE + 0x180u))
#define _CFD2FIFOSTA1_TFNRFNIF_MASK  0x00000001u
#define _CFD2FIFOSTA1_TXATIF_MASK    0x00000010u

/* CiFIFOUA1 */
#define CFD2FIFOUA1     (*(volatile uint32_t *)(_CFD2_BASE + 0x190u))

/* CiFIFOCON2 (FIFO2 = RX FIFO in plib_canfd2) */
#define CFD2FIFOCON2    (*(volatile uint32_t *)(_CFD2_BASE + 0x1A0u))
#define CFD2FIFOCON2SET (*(volatile uint32_t *)(_CFD2_BASE + 0x1A4u))
#define CFD2FIFOCON2CLR (*(volatile uint32_t *)(_CFD2_BASE + 0x1A8u))

#define _CFD2FIFOCON2_FSIZE_POSITION 24
#define _CFD2FIFOCON2_FSIZE_MASK     0x1F000000u
#define _CFD2FIFOCON2_PLSIZE_POSITION 29
#define _CFD2FIFOCON2_PLSIZE_MASK    0xE0000000u

/* CiFIFOSTA2 */
#define CFD2FIFOSTA2    (*(volatile uint32_t *)(_CFD2_BASE + 0x1B0u))
#define _CFD2FIFOSTA2_TFNRFNIF_MASK  0x00000001u

/* CiFIFOUA2 */
#define CFD2FIFOUA2     (*(volatile uint32_t *)(_CFD2_BASE + 0x1C0u))

/* CiFLTCON0 */
#define CFD2FLTCON0     (*(volatile uint32_t *)(_CFD2_BASE + 0x740u))
#define CFD2FLTCON0SET  (*(volatile uint32_t *)(_CFD2_BASE + 0x744u))

#define _CFD2FLTCON0_F0BP_POSITION   0
#define _CFD2FLTCON0_F0BP_MASK       0x0000001Fu
#define _CFD2FLTCON0_FLTEN0_POSITION 7
#define _CFD2FLTCON0_FLTEN0_MASK     0x00000080u

/* CiFLTOBJ0 */
#define CFD2FLTOBJ0     (*(volatile uint32_t *)(_CFD2_BASE + 0x7C0u))
#define _CFD2FLTOBJ0_EXIDE_MASK      0x20000000u

/* CiMASK0 */
#define CFD2MASK0       (*(volatile uint32_t *)(_CFD2_BASE + 0x7D0u))
#define _CFD2MASK0_MIDE_MASK         0x20000000u

/* -----------------------------------------------------------------------
 * IFS7 / IEC7  (sources 224–255)
 * USB2 = EVIC source 244 → IFS7/IEC7 bit 20
 * IFS7 at EVIC+0x00B0, IEC7 at EVIC+0x0130
 * ----------------------------------------------------------------------- */

#define IFS7            (*(volatile uint32_t *)(_EVIC_BASE + 0x00B0u))
#define IFS7CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x00B4u))
#define IFS7SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x00B8u))
#define IFS7INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x00BCu))

#define IEC7            (*(volatile uint32_t *)(_EVIC_BASE + 0x0130u))
#define IEC7CLR         (*(volatile uint32_t *)(_EVIC_BASE + 0x0134u))
#define IEC7SET         (*(volatile uint32_t *)(_EVIC_BASE + 0x0138u))
#define IEC7INV         (*(volatile uint32_t *)(_EVIC_BASE + 0x013Cu))

#define _IFS7_USB2IF_MASK   (1u << 20)   /* source 244 = IFS7 bit 20 */
#define _IEC7_USB2IE_MASK   (1u << 20)

/* USB1 interrupt: source 34 = IFS1 bit 2 */
#define _IFS1_USB1IF_MASK   (1u << 2)
#define _IEC1_USB1IE_MASK   (1u << 2)

/* -----------------------------------------------------------------------
 * USB1 registers  (base 0xBF889000, first user register at +0x040)
 * USB2 registers  (base 0xBF88A000, first user register at +0x040)
 *
 * _USB_BASE_ADDRESS / _USB2_BASE_ADDRESS match XC32 p32mk1024mcm100.h:
 *   USB1 = 0xBF889040,  USB2 = 0xBF88A040
 * USB_MODULE_ID is a typedef uint32_t (peripheral identified by base addr).
 * ----------------------------------------------------------------------- */

#define _USB_BASE_ADDRESS   0xBF889040u
#define _USB2_BASE_ADDRESS  0xBF88A040u
#define USB_NUMBER_OF_MODULES 2U

/* -----------------------------------------------------------------------
 * Vector numbers (used in configTICK_INTERRUPT_VECTOR)
 * ----------------------------------------------------------------------- */

#define _TIMER_1_VECTOR         4
#define _CORE_TIMER_VECTOR      0
#define _USB_1_VECTOR           34U
#define _USB_2_VECTOR           244U

/* -----------------------------------------------------------------------
 * USB register bit-field typedef structs
 * Extracted from XC32 p32mk1024mcm100.h (extern declarations omitted —
 * we use our own volatile-pointer macros for register access).
 * Required by driver/usb/usbfs/src/templates/usbfs_registers.h which
 * does #include <xc.h> and uses these types directly.
 * ----------------------------------------------------------------------- */

typedef struct {
  uint32_t VBUSVDIF:1;
  uint32_t :1;
  uint32_t SESENDIF:1;
  uint32_t SESVDIF:1;
  uint32_t ACTVIF:1;
  uint32_t LSTATEIF:1;
  uint32_t T1MSECIF:1;
  uint32_t IDIF:1;
} __U1OTGIRbits_t;

typedef struct {
  uint32_t VBUSVDIE:1;
  uint32_t :1;
  uint32_t SESENDIE:1;
  uint32_t SESVDIE:1;
  uint32_t ACTVIE:1;
  uint32_t LSTATEIE:1;
  uint32_t T1MSECIE:1;
  uint32_t IDIE:1;
} __U1OTGIEbits_t;

typedef struct {
  uint32_t VBUSVD:1;
  uint32_t :1;
  uint32_t SESEND:1;
  uint32_t SESVD:1;
  uint32_t :1;
  uint32_t LSTATE:1;
  uint32_t :1;
  uint32_t ID:1;
} __U1OTGSTATbits_t;

typedef struct {
  uint32_t VBUSDIS:1;
  uint32_t VBUSCHG:1;
  uint32_t OTGEN:1;
  uint32_t VBUSON:1;
  uint32_t DMPULDWN:1;
  uint32_t DPPULDWN:1;
  uint32_t DMPULUP:1;
  uint32_t DPPULUP:1;
} __U1OTGCONbits_t;

typedef struct {
  uint32_t USBPWR:1;
  uint32_t USUSPEND:1;
  uint32_t :1;
  uint32_t USBBUSY:1;
  uint32_t USLPGRD:1;
  uint32_t :2;
  uint32_t UACTPND:1;
} __U1PWRCbits_t;

typedef union {
  struct {
    uint32_t URSTIF_DETACHIF:1;
    uint32_t UERRIF:1;
    uint32_t SOFIF:1;
    uint32_t TRNIF:1;
    uint32_t IDLEIF:1;
    uint32_t RESUMEIF:1;
    uint32_t ATTACHIF:1;
    uint32_t STALLIF:1;
  };
  struct {
    uint32_t DETACHIF:1;
  };
  struct {
    uint32_t URSTIF:1;
  };
} __U1IRbits_t;

typedef union {
  struct {
    uint32_t URSTIE_DETACHIE:1;
    uint32_t UERRIE:1;
    uint32_t SOFIE:1;
    uint32_t TRNIE:1;
    uint32_t IDLEIE:1;
    uint32_t RESUMEIE:1;
    uint32_t ATTACHIE:1;
    uint32_t STALLIE:1;
  };
  struct {
    uint32_t DETACHIE:1;
  };
  struct {
    uint32_t URSTIE:1;
  };
} __U1IEbits_t;

typedef union {
  struct {
    uint32_t PIDEF:1;
    uint32_t CRC5EF_EOFEF:1;
    uint32_t CRC16EF:1;
    uint32_t DFN8EF:1;
    uint32_t BTOEF:1;
    uint32_t DMAEF:1;
    uint32_t BMXEF:1;
    uint32_t BTSEF:1;
  };
  struct {
    uint32_t :1;
    uint32_t CRC5EF:1;
  };
  struct {
    uint32_t :1;
    uint32_t EOFEF:1;
  };
} __U1EIRbits_t;

typedef union {
  struct {
    uint32_t PIDEE:1;
    uint32_t CRC5EE_EOFEE:1;
    uint32_t CRC16EE:1;
    uint32_t DFN8EE:1;
    uint32_t BTOEE:1;
    uint32_t DMAEE:1;
    uint32_t BMXEE:1;
    uint32_t BTSEE:1;
  };
  struct {
    uint32_t :1;
    uint32_t CRC5EE:1;
  };
  struct {
    uint32_t :1;
    uint32_t EOFEE:1;
  };
} __U1EIEbits_t;

typedef union {
  struct {
    uint32_t :2;
    uint32_t PPBI:1;
    uint32_t DIR:1;
    uint32_t ENDPT:4;
  };
  struct {
    uint32_t :4;
    uint32_t ENDPT0:1;
    uint32_t ENDPT1:1;
    uint32_t ENDPT2:1;
    uint32_t ENDPT3:1;
  };
} __U1STATbits_t;

typedef union {
  struct {
    uint32_t USBEN_SOFEN:1;
    uint32_t PPBRST:1;
    uint32_t RESUME:1;
    uint32_t HOSTEN:1;
    uint32_t USBRST:1;
    uint32_t PKTDIS_TOKBUSY:1;
    uint32_t SE0:1;
    uint32_t JSTATE:1;
  };
  struct {
    uint32_t USBEN:1;
  };
  struct {
    uint32_t SOFEN:1;
    uint32_t :4;
    uint32_t PKTDIS:1;
  };
  struct {
    uint32_t :5;
    uint32_t TOKBUSY:1;
  };
} __U1CONbits_t;

typedef union {
  struct {
    uint32_t DEVADDR:7;
    uint32_t LSPDEN:1;
  };
  struct {
    uint32_t DEVADDR0:1;
    uint32_t DEVADDR1:1;
    uint32_t DEVADDR2:1;
    uint32_t DEVADDR3:1;
    uint32_t DEVADDR4:1;
    uint32_t DEVADDR5:1;
    uint32_t DEVADDR6:1;
  };
} __U1ADDRbits_t;

typedef struct {
  uint32_t :1;
  uint32_t BDTPTRL:7;
} __U1BDTP1bits_t;

typedef union {
  struct {
    uint32_t FRML:8;
  };
  struct {
    uint32_t FRM0:1;
    uint32_t FRM1:1;
    uint32_t FRM2:1;
    uint32_t FRM3:1;
    uint32_t FRM4:1;
    uint32_t FRM5:1;
    uint32_t FRM6:1;
    uint32_t FRM7:1;
  };
} __U1FRMLbits_t;

typedef union {
  struct {
    uint32_t FRMH:3;
  };
  struct {
    uint32_t FRM8:1;
    uint32_t FRM9:1;
    uint32_t FRM10:1;
  };
} __U1FRMHbits_t;

typedef union {
  struct {
    uint32_t EP:4;
    uint32_t PID:4;
  };
  struct {
    uint32_t EP0:1;
  };
  struct {
    uint32_t :1;
    uint32_t EP1:1;
    uint32_t EP2:1;
    uint32_t EP3:1;
    uint32_t PID0:1;
  };
  struct {
    uint32_t :5;
    uint32_t PID1:1;
    uint32_t PID2:1;
    uint32_t PID3:1;
  };
} __U1TOKbits_t;

typedef struct {
  uint32_t CNT:8;
} __U1SOFbits_t;

typedef struct {
  uint32_t BDTPTRH:8;
} __U1BDTP2bits_t;

typedef struct {
  uint32_t BDTPTRU:8;
} __U1BDTP3bits_t;

typedef struct {
  uint32_t UASUSPND:1;
  uint32_t :2;
  uint32_t LSDEV:1;
  uint32_t USBSIDL:1;
  uint32_t :1;
  uint32_t UOEMON:1;
  uint32_t UTEYE:1;
} __U1CNFG1bits_t;

typedef struct {
  uint32_t EPHSHK:1;
  uint32_t EPSTALL:1;
  uint32_t EPTXEN:1;
  uint32_t EPRXEN:1;
  uint32_t EPCONDIS:1;
  uint32_t :1;
  uint32_t RETRYDIS:1;
  uint32_t LSPD:1;
} __U1EP0bits_t;

/* EP1–EP15 share the same layout (no RETRYDIS/LSPD fields) */
typedef struct {
  uint32_t EPHSHK:1;
  uint32_t EPSTALL:1;
  uint32_t EPTXEN:1;
  uint32_t EPRXEN:1;
  uint32_t EPCONDIS:1;
} __U1EP1bits_t, __U1EP2bits_t, __U1EP3bits_t, __U1EP4bits_t,
  __U1EP5bits_t, __U1EP6bits_t, __U1EP7bits_t, __U1EP8bits_t,
  __U1EP9bits_t, __U1EP10bits_t, __U1EP11bits_t, __U1EP12bits_t,
  __U1EP13bits_t, __U1EP14bits_t, __U1EP15bits_t;

/* -----------------------------------------------------------------------
 * USB register bit-mask and position constants
 * Extracted from XC32 p32mk1024mcm100.h for usbfs_registers.h compatibility.
 * ----------------------------------------------------------------------- */
#define _U1ADDR_DEVADDR_MASK                     0x0000007Fu
#define _U1ADDR_LSPDEN_MASK                      0x00000080u
#define _U1CNFG1_UASUSPND_MASK                   0x00000001u
#define _U1CNFG1_USBSIDL_MASK                    0x00000010u
#define _U1CNFG1_UTEYE_MASK                      0x00000080u
#define _U1CNFG1_UTEYE_POSITION                  0x00000007u
#define _U1CON_HOSTEN_MASK                       0x00000008u
#define _U1CON_PKTDIS_TOKBUSY_MASK               0x00000020u
#define _U1CON_PPBRST_MASK                       0x00000002u
#define _U1CON_RESUME_MASK                       0x00000004u
#define _U1CON_USBEN_SOFEN_MASK                  0x00000001u
#define _U1CON_USBEN_SOFEN_POSITION              0x00000000u
#define _U1CON_USBRST_MASK                       0x00000010u
#define _U1EP0_EPCONDIS_MASK                     0x00000010u
#define _U1EP0_EPCONDIS_POSITION                 0x00000004u
#define _U1EP0_EPHSHK_MASK                       0x00000001u
#define _U1EP0_EPHSHK_POSITION                   0x00000000u
#define _U1EP0_EPRXEN_MASK                       0x00000008u
#define _U1EP0_EPSTALL_MASK                      0x00000002u
#define _U1EP0_EPSTALL_POSITION                  0x00000001u
#define _U1EP0_EPTXEN_MASK                       0x00000004u
#define _U1EP0_LSPD_MASK                         0x00000080u
#define _U1EP0_RETRYDIS_MASK                     0x00000040u
#define _U1EP1_EPCONDIS_POSITION                 0x00000004u
#define _U1EP1_EPHSHK_POSITION                   0x00000000u
#define _U1EP1_EPSTALL_POSITION                  0x00000001u
#define _U1OTGCON_DMPULDWN_MASK                  0x00000010u
#define _U1OTGCON_DMPULUP_MASK                   0x00000040u
#define _U1OTGCON_DPPULDWN_MASK                  0x00000020u
#define _U1OTGCON_DPPULUP_MASK                   0x00000080u
#define _U1OTGCON_OTGEN_MASK                     0x00000004u
#define _U1OTGCON_VBUSCHG_MASK                   0x00000002u
#define _U1OTGCON_VBUSDIS_MASK                   0x00000001u
#define _U1OTGCON_VBUSON_MASK                    0x00000008u
#define _U1PWRC_USBPWR_MASK                      0x00000001u
#define _U1PWRC_USLPGRD_MASK                     0x00000010u
#define _U1PWRC_USUSPEND_MASK                    0x00000002u
#define _U1TOK_EP_MASK                           0x0000000Fu
#define _U1TOK_PID_POSITION                      0x00000004u

#endif /* __ASSEMBLER__ */
#endif /* XC_H */
