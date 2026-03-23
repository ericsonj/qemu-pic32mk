/*******************************************************************************
  TMR Peripheral Library Interface Source File — TMR7

  QEMU adaptation: Timer7 is EVIC source 32 → IFS1[0]/IEC1[0].
  Reference plib used IFS2/IEC2 (different PIC32MK silicon variant).
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
* SPDX-License-Identifier: LicenseRef-Microchip-Proprietary
*******************************************************************************/

#include "device.h"
#include "plib_tmr7.h"
#include "interrupts.h"

static volatile TMR_TIMER_OBJECT tmr7Obj;

void TMR7_Initialize(void) {
    T7CONCLR = _T7CON_ON_MASK;
    T7CONSET = 0x0;
    TMR7 = 0x0;
    PR7 = 0U;
    /* EVIC source 32 → IEC1[0] */
    IEC1SET = _IEC1_T7IE_MASK;
}

void TMR7_Start(void) {
    T7CONSET = _T7CON_ON_MASK;
}

void TMR7_Stop(void) {
    T7CONCLR = _T7CON_ON_MASK;
}

void TMR7_PeriodSet(uint16_t period) {
    PR7 = period;
}

uint16_t TMR7_PeriodGet(void) {
    return (uint16_t)PR7;
}

uint16_t TMR7_CounterGet(void) {
    return (uint16_t)(TMR7);
}

uint32_t TMR7_FrequencyGet(void) {
    return (0);
}

void __attribute__((used)) TIMER_7_InterruptHandler(void) {
    /* EVIC source 32 → IFS1[0] */
    uint32_t status = IFS1bits.T7IF;
    IFS1CLR = _IFS1_T7IF_MASK;

    if ((tmr7Obj.callback_fn != NULL)) {
        uintptr_t context = tmr7Obj.context;
        tmr7Obj.callback_fn(status, context);
    }
}

void TMR7_InterruptEnable(void) {
    IEC1SET = _IEC1_T7IE_MASK;
}

void TMR7_InterruptDisable(void) {
    IEC1CLR = _IEC1_T7IE_MASK;
}

void TMR7_CallbackRegister(TMR_CALLBACK callback_fn, uintptr_t context) {
    tmr7Obj.callback_fn = callback_fn;
    tmr7Obj.context = context;
}
