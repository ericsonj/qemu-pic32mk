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
 * Vector numbers (used in configTICK_INTERRUPT_VECTOR)
 * ----------------------------------------------------------------------- */

#define _TIMER_1_VECTOR         4
#define _CORE_TIMER_VECTOR      0

#endif /* __ASSEMBLER__ */
#endif /* XC_H */
