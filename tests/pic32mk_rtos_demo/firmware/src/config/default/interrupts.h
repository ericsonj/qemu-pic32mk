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

/* USB1 ISR — calls DRV_USBFS_Tasks_ISR internally */
void USB1_InterruptHandler(void);

/* Change Notice A ISR — dispatches GPIO_PinInterruptCallback for port A */
void CHANGE_NOTICE_A_InterruptHandler(void);

/* Timer1 ISR — clears IFS0.T1IF and calls TMR1_CallbackRegister callback */
void TIMER_1_InterruptHandler(void);

/* Timer2-9 ISRs — clears IFSx.TxIF and calls TMRx_CallbackRegister callback */
void TIMER_2_InterruptHandler(void);
void TIMER_3_InterruptHandler(void);
void TIMER_4_InterruptHandler(void);
void TIMER_5_InterruptHandler(void);
void TIMER_6_InterruptHandler(void);
void TIMER_7_InterruptHandler(void);
void TIMER_8_InterruptHandler(void);
void TIMER_9_InterruptHandler(void);

#endif /* INTERRUPTS_H */
