/*******************************************************************************
  TMR Peripheral Library Interface Source File

  Company
    Microchip Technology Inc.

  File Name
    plib_tmr6.c

  Summary
    TMR6 peripheral library source file.

  Description
    This file implements the interface to the TMR peripheral library.

    QEMU adaptation: Timer6 is EVIC source 28 → IFS0[28]/IEC0[28].
    The reference plib used IFS2/IEC2 (different PIC32MK silicon variant).

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#include "device.h"
#include "plib_tmr6.h"
#include "interrupts.h"

static volatile TMR_TIMER_OBJECT tmr6Obj;

void TMR6_Initialize(void) {
    /* Disable Timer */
    T6CONCLR = _T6CON_ON_MASK;

    T6CONSET = 0x0;

    /* Clear counter */
    TMR6 = 0x0;

    /* Set period */
    PR6 = 0U;

    /* EVIC source 28 → IEC0[28] */
    IEC0SET = _IEC0_T6IE_MASK;
}

void TMR6_Start(void) {
    T6CONSET = _T6CON_ON_MASK;
}

void TMR6_Stop(void) {
    T6CONCLR = _T6CON_ON_MASK;
}

void TMR6_PeriodSet(uint16_t period) {
    PR6 = period;
}

uint16_t TMR6_PeriodGet(void) {
    return (uint16_t)PR6;
}

uint16_t TMR6_CounterGet(void) {
    return (uint16_t)(TMR6);
}

uint32_t TMR6_FrequencyGet(void) {
    return (0);
}

void __attribute__((used)) TIMER_6_InterruptHandler(void) {
    /* EVIC source 28 → IFS0[28] */
    uint32_t status = IFS0bits.T6IF;
    IFS0CLR = _IFS0_T6IF_MASK;

    if ((tmr6Obj.callback_fn != NULL)) {
        uintptr_t context = tmr6Obj.context;
        tmr6Obj.callback_fn(status, context);
    }
}

void TMR6_InterruptEnable(void) {
    IEC0SET = _IEC0_T6IE_MASK;
}

void TMR6_InterruptDisable(void) {
    IEC0CLR = _IEC0_T6IE_MASK;
}

void TMR6_CallbackRegister(TMR_CALLBACK callback_fn, uintptr_t context) {
    tmr6Obj.callback_fn = callback_fn;
    tmr6Obj.context = context;
}
