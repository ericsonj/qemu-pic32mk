/*******************************************************************************
  SPI PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_spi_slave_common.h

  Summary:
    SPI PLIB Slave Common Header File

  Description:
    This file has prototype of all the interfaces which are common for all the
    SPI peripherals.

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019-2020 Microchip Technology Inc. and its subsidiaries.
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

#ifndef PLIB_SPI_SLAVE_COMMON_H
#define PLIB_SPI_SLAVE_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* Provide C++ Compatibility */
#ifdef __cplusplus

extern "C" {
#endif

/****************************** SPI Slave Interface *********************************/

/* Error status when no error has occurred */
#define    SPI_SLAVE_ERROR_NONE     (0U)

/* Buffer overflow error has occured */
#define    SPI_SLAVE_ERROR_BUFOVF  (0x00000040U)

typedef uint32_t SPI_SLAVE_ERROR;

/* SPI Slave Mode CallBack Function Pointer */

typedef void (*SPI_SLAVE_CALLBACK)(uintptr_t context);

// *****************************************************************************
// *****************************************************************************
// Section: Local: **** Do Not Use ****
// *****************************************************************************
// *****************************************************************************

typedef struct
{
    /* Exchange busy status of the SPI */
    bool transferIsBusy;

    /* SPI Event handler */
    SPI_SLAVE_CALLBACK callback;

    /* Context */
    uintptr_t context;

    SPI_SLAVE_ERROR errorStatus;

    /* Number of bytes to write in the transmit buffer */
    uint32_t nWrBytes;

    /* Index to the number of bytes already written out from the transmit buffer */
    uint32_t wrOutIndex;

    /* Index into the receive buffer where the next received byte will be copied */
    uint32_t rdInIndex;

    /* Flag to indicate that the RX interrupt is active (being serviced) */
    bool rxInterruptActive;

    /* Flag to indicate that CS interrupt has delegated callback responsibility to the SPI receive interrupt */
    bool csInterruptPending;
} SPI_SLAVE_OBJECT;

/* Provide C++ Compatibility */
#ifdef __cplusplus
}
#endif

#endif // PLIB_SPI_SLAVE_COMMON_H
