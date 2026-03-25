/*******************************************************************************
  Output Compare OCMP16 Peripheral Library (PLIB)

  Company:
    Microchip Technology Inc.

  File Name:
    plib_ocmp16.c

  Summary:
    OCMP16 Source File

  Description:
    None

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
#include "plib_ocmp16.h"
#include "interrupts.h"

// *****************************************************************************

// *****************************************************************************
// Section: OCMP16 Implementation
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************


void OCMP16_Initialize (void)
{
    /*Setup OC16CON        */
    /*OCM         = 6        */
    /*OCTSEL       = 0        */
    /*OC32         = 0        */
    /*SIDL         = false    */

    OC16CON = 0x6;

    OC16R = 0;
    OC16RS = 0;

}

void OCMP16_Enable (void)
{
    OC16CONSET = _OC16CON_ON_MASK;
}

void OCMP16_Disable (void)
{
    OC16CONCLR = _OC16CON_ON_MASK;
}



uint16_t OCMP16_CompareValueGet (void)
{
    return (uint16_t)OC16R;
}

void OCMP16_CompareSecondaryValueSet (uint16_t value)
{
    OC16RS = value;
}

uint16_t OCMP16_CompareSecondaryValueGet (void)
{
    return (uint16_t)OC16RS;
}

