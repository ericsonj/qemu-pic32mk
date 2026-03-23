/*******************************************************************************
  TMR Peripheral Library Interface Source File — TMR9

  QEMU adaptation: Timer9 is EVIC source 40 → IFS1[8]/IEC1[8].
  Reference plib used IFS2/IEC2 (different PIC32MK silicon variant).
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
* SPDX-License-Identifier: LicenseRef-Microchip-Proprietary
*******************************************************************************/

#include "device.h"
#include "plib_tmr9.h"
#include "interrupts.h"

static volatile TMR_TIMER_OBJECT tmr9Obj;

void TMR9_Initialize(void) {
    T9CONCLR = _T9CON_ON_MASK;
    T9CONSET = 0x0;
    TMR9 = 0x0;
    PR9 = 0U;
    /* EVIC source 40 → IEC1[8] */
    IEC1SET = _IEC1_T9IE_MASK;
}

void TMR9_Start(void) {
    T9CONSET = _T9CON_ON_MASK;
}

void TMR9_Stop(void) {
    T9CONCLR = _T9CON_ON_MASK;
}

void TMR9_PeriodSet(uint16_t period) {
    PR9 = period;
}

uint16_t TMR9_PeriodGet(void) {
    return (uint16_t)PR9;
}

uint16_t TMR9_CounterGet(void) {
    return (uint16_t)(TMR9);
}

uint32_t TMR9_FrequencyGet(void) {
    return (0);
}

void __attribute__((used)) TIMER_9_InterruptHandler(void) {
    /* EVIC source 40 → IFS1[8] */
    uint32_t status = IFS1bits.T9IF;
    IFS1CLR = _IFS1_T9IF_MASK;

    if ((tmr9Obj.callback_fn != NULL)) {
        uintptr_t context = tmr9Obj.context;
        tmr9Obj.callback_fn(status, context);
    }
}

void TMR9_InterruptEnable(void) {
    IEC1SET = _IEC1_T9IE_MASK;
}

void TMR9_InterruptDisable(void) {
    IEC1CLR = _IEC1_T9IE_MASK;
}

void TMR9_CallbackRegister(TMR_CALLBACK callback_fn, uintptr_t context) {
    tmr9Obj.callback_fn = callback_fn;
    tmr9Obj.context = context;
}
