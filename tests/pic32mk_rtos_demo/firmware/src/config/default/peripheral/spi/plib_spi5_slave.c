/*******************************************************************************
  SPI PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_spi5_slave.c

  Summary:
    SPI5 Slave Source File

  Description:
    This file has implementation of all the interfaces provided for particular
    SPI peripheral.

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

#include "plib_spi5_slave.h"
#include "peripheral/gpio/plib_gpio.h"
#include <string.h>
#include "interrupts.h"

// *****************************************************************************
// *****************************************************************************
// Section: SPI5 Slave Implementation
// *****************************************************************************
// *****************************************************************************
#define SPI5_CS_PIN                      GPIO_PIN_RA7

#define SPI5_READ_BUFFER_SIZE            256
#define SPI5_WRITE_BUFFER_SIZE           256

static volatile uint8_t SPI5_ReadBuffer[SPI5_READ_BUFFER_SIZE];
static volatile uint8_t SPI5_WriteBuffer[SPI5_WRITE_BUFFER_SIZE];

/* Global object to save SPI Exchange related data */
static volatile SPI_SLAVE_OBJECT spi5Obj;

#define SPI5_CON_CKP                        (0UL << _SPI5CON_CKP_POSITION)
#define SPI5_CON_CKE                        (1UL << _SPI5CON_CKE_POSITION)
#define SPI5_CON_MODE_32_MODE_16            (0UL << _SPI5CON_MODE16_POSITION)
#define SPI5_CON_ENHBUF                     (1UL << _SPI5CON_ENHBUF_POSITION)
#define SPI5_CON_STXISEL                    (3UL << _SPI5CON_STXISEL_POSITION)
#define SPI5_CON_SRXISEL                    (1UL << _SPI5CON_SRXISEL_POSITION)
#define SPI5_CON_SSEN                       (1UL << _SPI5CON_SSEN_POSITION)

#define SPI5_ENABLE_RX_INT()                IEC5SET = 0x00000400u
#define SPI5_CLEAR_RX_INT_FLAG()            IFS5CLR = 0x00000400u

#define SPI5_DISABLE_TX_INT()               IEC5CLR = 0x00000800u
#define SPI5_ENABLE_TX_INT()                IEC5SET = 0x00000800u
#define SPI5_CLEAR_TX_INT_FLAG()            IFS5CLR = 0x00000800u

#define SPI5_ENABLE_ERR_INT()               IEC5SET = 0x00000200u
#define SPI5_CLEAR_ERR_INT_FLAG()           IFS5CLR = 0x00000200u

/* Forward declarations */
static void SPI5_CS_Handler(GPIO_PIN pin, uintptr_t context);

static void mem_copy(volatile void *pDst, volatile void *pSrc, uint32_t nBytes) {
    volatile uint8_t *pSource = (volatile uint8_t *)pSrc;
    volatile uint8_t *pDest = (volatile uint8_t *)pDst;

    for (uint32_t i = 0U; i < nBytes; i++) {
        pDest[i] = pSource[i];
    }
}

void SPI5_Initialize(void) {
    /* Disable SPI5 Interrupts */
    IEC5CLR = 0x00000E00u;

    /* STOP and Reset the SPI */
    SPI5CON = 0;

    /* Clear SPI5 Interrupt flags */
    IFS5CLR = 0x00000E00u;

    /* CLear the receiver overflow error flag */
    SPI5STATCLR = _SPI5STAT_SPIROV_MASK;

    /*
    SRXISEL = 1 (Receive buffer is not empty)
    STXISEL = 3 (Transmit buffer is not full)
    MSTEN = 0
    CKP = 0
    CKE = 1
    MODE< 32,16 > = 0
    ENHBUF = 1
    */

    SPI5CONSET = (SPI5_CON_ENHBUF | SPI5_CON_MODE_32_MODE_16 | SPI5_CON_CKE |
                 SPI5_CON_CKP | SPI5_CON_SSEN | SPI5_CON_STXISEL | SPI5_CON_SRXISEL);

    /* Enable generation of interrupt on receiver overflow */
    SPI5CON2SET = _SPI5CON2_SPIROVEN_MASK;

    spi5Obj.rdInIndex = 0;
    spi5Obj.wrOutIndex = 0;
    spi5Obj.nWrBytes = 0;
    spi5Obj.errorStatus = SPI_SLAVE_ERROR_NONE;
    spi5Obj.callback = NULL;
    spi5Obj.transferIsBusy = false;
    spi5Obj.csInterruptPending = false;
    spi5Obj.rxInterruptActive = false;

    /* Register callback and enable notifications on Chip Select logic level change */
    (void)GPIO_PinInterruptCallbackRegister(SPI5_CS_PIN, SPI5_CS_Handler, 0U);
    GPIO_PinInterruptEnable(SPI5_CS_PIN);

    /* Enable SPI5 RX and Error Interrupts. TX interrupt will be enabled when a SPI write is submitted. */
    SPI5_ENABLE_RX_INT();
    SPI5_ENABLE_ERR_INT();

    /* Enable SPI5 */
    SPI5CONSET = _SPI5CON_ON_MASK;
}

/* For 16-bit/32-bit mode, the "size" must be specified in terms of 16-bit/32-bit words */
size_t SPI5_Read(void *pRdBuffer, size_t size) {
    size_t rdSize = size;
    uint32_t rdInIndex = spi5Obj.rdInIndex;

    if (rdSize > rdInIndex) {
        rdSize = rdInIndex;
    }

    (void) mem_copy(pRdBuffer, SPI5_ReadBuffer, rdSize);

    /* QEMU-friendly behavior: clear the RX buffer index after read. */
    spi5Obj.rdInIndex = 0;

    return rdSize;
}

/* For 16-bit/32-bit mode, the "size" must be specified in terms of 16-bit/32-bit words */
size_t SPI5_Write(void *pWrBuffer, size_t size) {
    size_t wrSize = size;
    size_t wrOutIndex = 0;

    SPI5_DISABLE_TX_INT();

    if (wrSize > SPI5_WRITE_BUFFER_SIZE) {
        wrSize = SPI5_WRITE_BUFFER_SIZE;
    }

    (void) mem_copy(SPI5_WriteBuffer, pWrBuffer, wrSize);

    spi5Obj.nWrBytes = wrSize;

    /* Fill up the FIFO as long as there are empty elements */
    while ((!(SPI5STAT & _SPI5STAT_SPITBF_MASK)) && (wrOutIndex < wrSize)) {
        SPI5BUF = SPI5_WriteBuffer[wrOutIndex];
        wrOutIndex++;
    }

    spi5Obj.wrOutIndex = wrOutIndex;

    /* Enable TX interrupt */
    SPI5_ENABLE_TX_INT();

    return wrSize;
}

/* For 16-bit/32-bit mode, the return value is in terms of 16-bit/32-bit words */
size_t SPI5_ReadCountGet(void) {
    return spi5Obj.rdInIndex;
}

/* For 16-bit/32-bit mode, the return value is in terms of 16-bit/32-bit words */
size_t SPI5_ReadBufferSizeGet(void) {
    return SPI5_READ_BUFFER_SIZE;
}

/* For 16-bit/32-bit mode, the return value is in terms of 16-bit/32-bit words */
size_t SPI5_WriteBufferSizeGet(void) {
    return SPI5_WRITE_BUFFER_SIZE;
}

void SPI5_CallbackRegister(SPI_SLAVE_CALLBACK callBack, uintptr_t context) {
    spi5Obj.callback = callBack;

    spi5Obj.context = context;
}

/* The status is returned as busy when CS is asserted */
bool SPI5_IsBusy(void) {
    return spi5Obj.transferIsBusy;
}

SPI_SLAVE_ERROR SPI5_ErrorGet(void) {
    SPI_SLAVE_ERROR errorStatus = spi5Obj.errorStatus;

    spi5Obj.errorStatus = SPI_SLAVE_ERROR_NONE;

    return errorStatus;
}

static void __attribute__((used)) SPI5_CS_Handler(GPIO_PIN pin, uintptr_t context) {
    bool activeState = false;

    if (GPIO_PinRead((GPIO_PIN)SPI5_CS_PIN) == activeState) {
        /* CS is asserted */
        spi5Obj.transferIsBusy = true;
    }
    else {
        /* Give application callback only if RX interrupt is not preempted and RX interrupt is not pending to be serviced */

        bool rxInterruptActive = spi5Obj.rxInterruptActive;

        if (((IFS5 & _IFS5_SPI5RXIF_MASK) == 0) && (rxInterruptActive == false)) {
            /* CS is de-asserted */
            spi5Obj.transferIsBusy = false;

            spi5Obj.wrOutIndex = 0;
            spi5Obj.nWrBytes = 0;

            if (spi5Obj.callback != NULL) {
                uintptr_t context_val = spi5Obj.context;

                spi5Obj.callback(context_val);
            }

            /* Clear the read index. Application must read out the data by calling SPI5_Read API in the callback */
            spi5Obj.rdInIndex = 0;
        }
        else {
            /* If CS interrupt is serviced by either preempting the RX interrupt or RX interrupt is pending to be serviced,
             * then delegate the responsibility of giving application callback to the RX interrupt handler */

            spi5Obj.csInterruptPending = true;
        }
    }
}

void __attribute__((used)) SPI5_FAULT_InterruptHandler(void) {
    spi5Obj.errorStatus = (SPI5STAT & _SPI5STAT_SPIROV_MASK);

    /* Clear the receive overflow flag */
    SPI5STATCLR = _SPI5STAT_SPIROV_MASK;

    SPI5_CLEAR_ERR_INT_FLAG();
}

void __attribute__((used)) SPI5_TX_InterruptHandler(void) {
    size_t wrOutIndex = spi5Obj.wrOutIndex;
    size_t nWrBytes = spi5Obj.nWrBytes;

    /* Fill up the FIFO as long as there are empty elements */
    while ((!(SPI5STAT & _SPI5STAT_SPITBF_MASK)) && (wrOutIndex < nWrBytes)) {
        SPI5BUF = SPI5_WriteBuffer[wrOutIndex];
        wrOutIndex++;
    }

    spi5Obj.wrOutIndex = wrOutIndex;

    /* Clear the transmit interrupt flag */
    SPI5_CLEAR_TX_INT_FLAG();

    if (spi5Obj.wrOutIndex == nWrBytes) {
        /* Nothing to transmit. Disable transmit interrupt. The last byte sent by the master will be shifted out automatically*/
        SPI5_DISABLE_TX_INT();
    }
}

void __attribute__((used)) SPI5_RX_InterruptHandler(void) {
    uint32_t receivedData = 0;

    spi5Obj.rxInterruptActive = true;

    size_t rdInIndex = spi5Obj.rdInIndex;

    while (!(SPI5STAT & _SPI5STAT_SPIRBE_MASK)) {
        /* Receive buffer is not empty. Read the received data. */
        receivedData = SPI5BUF;

        if (rdInIndex < SPI5_READ_BUFFER_SIZE) {
            SPI5_ReadBuffer[rdInIndex] = (uint8_t)receivedData;
            rdInIndex++;
        }
    }

    spi5Obj.rdInIndex = rdInIndex;

    /* Clear the receive interrupt flag */
    SPI5_CLEAR_RX_INT_FLAG();

    spi5Obj.rxInterruptActive = false;

    /* Check if CS interrupt occured before the RX interrupt and that CS interrupt delegated the responsibility to give
     * application callback to the RX interrupt */

    if (spi5Obj.csInterruptPending == true) {
        spi5Obj.csInterruptPending = false;
        spi5Obj.transferIsBusy = false;

        spi5Obj.wrOutIndex = 0;
        spi5Obj.nWrBytes = 0;

        if (spi5Obj.callback != NULL) {
            uintptr_t context = spi5Obj.context;

            spi5Obj.callback(context);
        }

        /* Clear the read index. Application must read out the data by calling SPI5_Read API in the callback */
        spi5Obj.rdInIndex = 0;
    }
}
