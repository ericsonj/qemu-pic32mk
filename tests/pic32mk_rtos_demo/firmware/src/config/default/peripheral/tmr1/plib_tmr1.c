/*******************************************************************************
  TMR1 Peripheral Library Interface Source File

  Company
    Microchip Technology Inc.

  File Name
    plib_tmr1.c

  Summary
    TMR1 peripheral library source file.

  Description
    This file implements the interface to the TMR1 peripheral library.

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
#include "plib_tmr1.h"
#include "interrupts.h"

static volatile TMR1_TIMER_OBJECT tmr1Obj;

/*
 * TMR1_Initialize — bring the timer to a known stopped state.
 *
 * Note: prescaler (TCKPS) and period (PR1) are NOT configured here.
 * Call TMR1_PeriodSet() and set T1CON.TCKPS before TMR1_Start().
 * For the FreeRTOS tick this is done in vApplicationSetupTickTimerInterrupt().
 */
void TMR1_Initialize(void)
{
    /* Disable Timer */
    T1CONCLR = _T1CON_ON_MASK;

    /* Clear counter */
    TMR1 = 0x0;

    /* Period = 0 (caller must set via TMR1_PeriodSet before starting) */
    PR1 = 0;

    /* Enable interrupt so the ISR fires when the timer is started */
    TMR1_InterruptEnable();
}

void TMR1_Start(void)
{
    T1CONSET = _T1CON_ON_MASK;
}

void TMR1_Stop(void)
{
    T1CONCLR = _T1CON_ON_MASK;
}

void TMR1_PeriodSet(uint16_t period)
{
    PR1 = period;
}

uint16_t TMR1_PeriodGet(void)
{
    return (uint16_t)PR1;
}

uint16_t TMR1_CounterGet(void)
{
    return (uint16_t)TMR1;
}

uint32_t TMR1_FrequencyGet(void)
{
    /* Frequency depends on SYSCLK and TCKPS; not tracked here. */
    return 0;
}

void TMR1_InterruptEnable(void)
{
    IEC0SET = _IEC0_T1IE_MASK;
}

void TMR1_InterruptDisable(void)
{
    IEC0CLR = _IEC0_T1IE_MASK;
}

void TMR1_CallbackRegister(TMR1_CALLBACK callback_fn, uintptr_t context)
{
    tmr1Obj.callback_fn = callback_fn;
    tmr1Obj.context     = context;
}

/*
 * TIMER_1_InterruptHandler — called by the ISR wrapper in port_asm_patched.S
 * when Timer1 fires.
 *
 * Note: when FreeRTOS is active this handler is NOT used for the scheduler
 * tick — FreeRTOS installs its own handler (vPortTickInterruptHandler) at
 * the T1 vector and clears IFS0.T1IF itself.  TIMER_1_InterruptHandler is
 * only reached if the application registers a callback via
 * TMR1_CallbackRegister() and routes the vector here instead.
 */
void __attribute__((used)) TIMER_1_InterruptHandler(void)
{
    uint32_t status = IFS0bits.T1IF;
    IFS0CLR = _IFS0_T1IF_MASK;

    if (tmr1Obj.callback_fn != NULL)
    {
        uintptr_t context = tmr1Obj.context;
        tmr1Obj.callback_fn(status, context);
    }
}
