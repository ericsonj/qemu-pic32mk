/*******************************************************************************
  UART2 PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_uart2.h

  Summary:
    UART2 PLIB Header File

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/

#ifndef PLIB_UART2_H
#define PLIB_UART2_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "plib_uart_common.h"

#ifdef __cplusplus
    extern "C" {
#endif

#define UART2_FrequencyGet()    (uint32_t)(120000000UL)

void UART2_Initialize( void );

bool UART2_SerialSetup( UART_SERIAL_SETUP *setup, uint32_t srcClkFreq );

UART_ERROR UART2_ErrorGet( void );

bool UART2_AutoBaudQuery( void );

void UART2_AutoBaudSet( bool enable );

size_t UART2_Write(uint8_t* pWrBuffer, const size_t size );

size_t UART2_WriteCountGet(void);

size_t UART2_WriteFreeBufferCountGet(void);

size_t UART2_WriteBufferSizeGet(void);

bool UART2_TransmitComplete(void);

bool UART2_WriteNotificationEnable(bool isEnabled, bool isPersistent);

void UART2_WriteThresholdSet(uint32_t nBytesThreshold);

void UART2_WriteCallbackRegister( UART_RING_BUFFER_CALLBACK callback, uintptr_t context);

size_t UART2_Read(uint8_t* pRdBuffer, const size_t size);

size_t UART2_ReadCountGet(void);

size_t UART2_ReadFreeBufferCountGet(void);

size_t UART2_ReadBufferSizeGet(void);

bool UART2_ReadNotificationEnable(bool isEnabled, bool isPersistent);

void UART2_ReadThresholdSet(uint32_t nBytesThreshold);

void UART2_ReadCallbackRegister( UART_RING_BUFFER_CALLBACK callback, uintptr_t context);

#ifdef __cplusplus
    }
#endif

#endif /* PLIB_UART2_H */
