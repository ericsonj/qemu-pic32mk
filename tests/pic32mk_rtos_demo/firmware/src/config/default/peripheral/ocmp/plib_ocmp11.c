/*******************************************************************************
  Output Compare OCMP11 Peripheral Library (PLIB)

  Company:
    Microchip Technology Inc.

  File Name:
    plib_ocmp11.c

  Summary:
    OCMP11 Source File

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
#include "plib_ocmp11.h"
#include "interrupts.h"

// *****************************************************************************

// *****************************************************************************
// Section: OCMP11 Implementation
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************


void OCMP11_Initialize (void)
{
    /*Setup OC11CON        */
    /*OCM         = 6        */
    /*OCTSEL       = 0        */
    /*OC32         = 0        */
    /*SIDL         = false    */

    OC11CON = 0x6;

    OC11R = 0;
    OC11RS = 0;

}

void OCMP11_Enable (void)
{
    OC11CONSET = _OC11CON_ON_MASK;
}

void OCMP11_Disable (void)
{
    OC11CONCLR = _OC11CON_ON_MASK;
}



uint16_t OCMP11_CompareValueGet (void)
{
    return (uint16_t)OC11R;
}

void OCMP11_CompareSecondaryValueSet (uint16_t value)
{
    OC11RS = value;
}

uint16_t OCMP11_CompareSecondaryValueGet (void)
{
    return (uint16_t)OC11RS;
}

