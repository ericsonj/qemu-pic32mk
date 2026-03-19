/*
 * interrupts.h — QEMU-side shim for Microchip Harmony's "interrupts.h"
 *
 * Declares the UART1 ISR prototypes that plib_uart1.c defines.
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void UART1_FAULT_InterruptHandler(void);
void UART1_RX_InterruptHandler(void);
void UART1_TX_InterruptHandler(void);

#endif /* INTERRUPTS_H */
