/*
 * interrupts.h — QEMU-side shim for Microchip Harmony's "interrupts.h"
 *
 * Declares ISR prototypes for UART1 and UART2 plibs.
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void UART1_FAULT_InterruptHandler(void);
void UART1_RX_InterruptHandler(void);
void UART1_TX_InterruptHandler(void);

void UART2_FAULT_InterruptHandler(void);
void UART2_RX_InterruptHandler(void);
void UART2_TX_InterruptHandler(void);

void CAN1_InterruptHandler(void);
void CAN2_InterruptHandler(void);

#endif /* INTERRUPTS_H */
