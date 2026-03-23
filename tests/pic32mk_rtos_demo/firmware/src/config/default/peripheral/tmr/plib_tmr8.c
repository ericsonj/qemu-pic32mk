/*******************************************************************************
  TMR Peripheral Library Interface Source File — TMR8

  QEMU adaptation: Timer8 is EVIC source 36 → IFS1[4]/IEC1[4].
  Reference plib used IFS2/IEC2 (different PIC32MK silicon variant).
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
* SPDX-License-Identifier: LicenseRef-Microchip-Proprietary
*******************************************************************************/

#include "device.h"
#include "plib_tmr8.h"
#include "interrupts.h"

static volatile TMR_TIMER_OBJECT tmr8Obj;

void TMR8_Initialize(void) {
    T8CONCLR = _T8CON_ON_MASK;
    T8CONSET = 0x0;
    TMR8 = 0x0;
    PR8 = 0U;
    /* EVIC source 36 → IEC1[4] */
    IEC1SET = _IEC1_T8IE_MASK;
}

void TMR8_Start(void) {
    T8CONSET = _T8CON_ON_MASK;
}

void TMR8_Stop(void) {
    T8CONCLR = _T8CON_ON_MASK;
}

void TMR8_PeriodSet(uint16_t period) {
    PR8 = period;
}

uint16_t TMR8_PeriodGet(void) {
    return (uint16_t)PR8;
}

uint16_t TMR8_CounterGet(void) {
    return (uint16_t)(TMR8);
}

uint32_t TMR8_FrequencyGet(void) {
    return (0);
}

void __attribute__((used)) TIMER_8_InterruptHandler(void) {
    /* EVIC source 36 → IFS1[4] */
    uint32_t status = IFS1bits.T8IF;
    IFS1CLR = _IFS1_T8IF_MASK;

    if ((tmr8Obj.callback_fn != NULL)) {
        uintptr_t context = tmr8Obj.context;
        tmr8Obj.callback_fn(status, context);
    }
}

void TMR8_InterruptEnable(void) {
    IEC1SET = _IEC1_T8IE_MASK;
}

void TMR8_InterruptDisable(void) {
    IEC1CLR = _IEC1_T8IE_MASK;
}

void TMR8_CallbackRegister(TMR_CALLBACK callback_fn, uintptr_t context) {
    tmr8Obj.callback_fn = callback_fn;
    tmr8Obj.context = context;
}
