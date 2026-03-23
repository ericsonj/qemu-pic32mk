/*******************************************************************************
  SYS CLK Static Functions for Clock System Service

  Company:
    Microchip Technology Inc.

  File Name:
    plib_clk.c

  Summary:
    SYS CLK static function implementations for the Clock System Service.

  Description:
    The Clock System Service provides a simple interface to manage the
    oscillators on Microchip microcontrollers. This file defines the static
    implementation for the Clock System Service.

  Remarks:
    Static functions incorporate all system clock configuration settings as
    determined by the user via the Microchip Harmony Configurator GUI.
    It provides static version of the routines, eliminating the need for an
    object ID or object handle.

    Static single-open interfaces also eliminate the need for the open handle.

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

// *****************************************************************************
// *****************************************************************************
// Section: Include Files
// *****************************************************************************
// *****************************************************************************

#include "device.h"
#include "plib_clk.h"

// *****************************************************************************
// *****************************************************************************
// Section: File Scope Functions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Function:
    void CLK_Initialize( void )

  Summary:
    Initializes hardware and internal data structure of the System Clock.

  Description:
    This function initializes the hardware and internal data structure of System
    Clock Service.

  Remarks:
    This is configuration values for the static version of the Clock System
    Service module is determined by the user via the MHC GUI.

    The objective is to eliminate the user's need to be knowledgeable in the
    function of the 'configuration bits' to configure the system oscillators.
*/

void CLK_Initialize( void )
{
    /* unlock system for clock configuration */
    SYSKEY = 0x00000000U;
    SYSKEY = 0xAA996655U;
    SYSKEY = 0x556699AAU;
    
    /* Peripheral Module Disable Configuration */

    CFGCONbits.PMDLOCK = 0;

    PMD1 = 0x170U;
    PMD2 = 0x17001fU;
    PMD3 = 0xfffff3ffU;
    PMD4 = 0xfff0000U;
    PMD5 = 0x20f2f3dU;
    PMD6 = 0xf0d0000U;
    PMD7 = 0x0U;

    CFGCONbits.PMDLOCK = 1;

 
      
    /* Configure UPLL */
    /* UPOSCEN = UPLL */
    /* PLLODIV = DIV_8 */
    /* PLLMULT = MUL_32 */
    /* PLLIDIV = DIV_1 */
    /* PLLRANGE = RANGE_8_16_MHZ */
    UPLLCON = 0x31f0002;

    /* Peripheral Bus 2 is by default enabled, set its divisor */
    PB2DIVbits.PBDIV = 0;

    /* Disable Peripheral Bus 6 */
    PB6DIVCLR = 0x00008000;

    /* Set up Reference Clock 1 */
    /* REFO1CON register */
    /* ROSEL =  SYSCLK */
    /* DIVSWEN = 1 */
    /* RODIV = 1 */
    REFO1CON = 0x10200;

    /* REFO1TRIM register */
    /* ROTRIM = 0 */
    REFO1TRIM = 0x0;

    /* Enable oscillator (ON bit) */
    REFO1CONSET = 0x00008000;

    /* Set up Reference Clock 2 */
    /* REFO2CON register */
    /* ROSEL =  SYSCLK */
    /* DIVSWEN = 1 */
    /* RODIV = 0 */
    REFO2CON = 0x200;

    /* REFO2TRIM register */
    /* ROTRIM = 0 */
    REFO2TRIM = 0x0;

    /* Enable oscillator (ON bit) */
    REFO2CONSET = 0x00008000;

    /* Set up Reference Clock 4 */
    /* REFO4CON register */
    /* ROSEL =  SYSCLK */
    /* DIVSWEN = 1 */
    /* RODIV = 1 */
    REFO4CON = 0x10200;

    /* REFO4TRIM register */
    /* ROTRIM = 256 */
    REFO4TRIM = 0x80000000;

    /* Enable oscillator (ON bit) */
    REFO4CONSET = 0x00008000;

  

    /* Lock system since done with clock configuration */
    SYSKEY = 0x33333333U;
}
