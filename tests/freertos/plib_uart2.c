/*******************************************************************************
  UART2 PLIB — PIC32MK QEMU

  Adapted from Microchip Harmony plib_uart2.c for the QEMU PIC32MK target.

  UART2 EVIC sources (DS60001519E Table 8-1):
    U2E  = source 115  → IFS3 bit 19,  IEC3 bit 19
    U2RX = source 116  → IFS3 bit 20,  IEC3 bit 20
    U2TX = source 117  → IFS3 bit 21,  IEC3 bit 21

  UART2 SFR base: 0xBF828200
*******************************************************************************/

#include "device.h"
#include "plib_uart2.h"
#include "interrupts.h"

static volatile UART_RING_BUFFER_OBJECT uart2Obj;

#define UART2_READ_BUFFER_SIZE      (128U)
#define UART2_READ_BUFFER_SIZE_9BIT (128U >> 1)
#define UART2_RX_INT_DISABLE()      IEC3CLR = _IEC3_U2RXIE_MASK
#define UART2_RX_INT_ENABLE()       IEC3SET = _IEC3_U2RXIE_MASK

static volatile uint8_t UART2_ReadBuffer[UART2_READ_BUFFER_SIZE];

#define UART2_WRITE_BUFFER_SIZE      (128U)
#define UART2_WRITE_BUFFER_SIZE_9BIT (128U >> 1)
#define UART2_TX_INT_DISABLE()       IEC3CLR = _IEC3_U2TXIE_MASK
#define UART2_TX_INT_ENABLE()        IEC3SET = _IEC3_U2TXIE_MASK

static volatile uint8_t UART2_WriteBuffer[UART2_WRITE_BUFFER_SIZE];

#define UART2_IS_9BIT_MODE_ENABLED() \
    (((U2MODE) & (_U2MODE_PDSEL0_MASK | _U2MODE_PDSEL1_MASK)) == \
     (_U2MODE_PDSEL0_MASK | _U2MODE_PDSEL1_MASK) ? true : false)

static void UART2_ErrorClear(void)
{
    UART_ERROR errors = UART_ERROR_NONE;
    uint8_t dummyData = 0u;

    errors = (UART_ERROR)(U2STA & (_U2STA_OERR_MASK | _U2STA_FERR_MASK | _U2STA_PERR_MASK));

    if (errors != UART_ERROR_NONE)
    {
        if ((U2STA & _U2STA_OERR_MASK) != 0U)
        {
            U2STACLR = _U2STA_OERR_MASK;
        }

        while ((U2STA & _U2STA_URXDA_MASK) != 0U)
        {
            dummyData = (uint8_t)U2RXREG;
        }

        IFS3CLR = _IFS3_U2EIF_MASK;
        IFS3CLR = _IFS3_U2RXIF_MASK;
    }

    (void)dummyData;
}

void UART2_Initialize(void)
{
    U2MODE = 0x8;   /* BRGH=1, 8N1 */

    U2STASET = (_U2STA_UTXEN_MASK | _U2STA_URXEN_MASK | _U2STA_UTXISEL1_MASK);

    /* 115200 baud @ 120 MHz, BRGH=1: BRG = (120e6 / (4*115200)) - 1 = 259 */
    U2BRG = 259;

    IEC3CLR = _IEC3_U2TXIE_MASK;

    uart2Obj.rdCallback = NULL;
    uart2Obj.rdInIndex = 0;
    uart2Obj.rdOutIndex = 0;
    uart2Obj.isRdNotificationEnabled = false;
    uart2Obj.isRdNotifyPersistently = false;
    uart2Obj.rdThreshold = 0;

    uart2Obj.wrCallback = NULL;
    uart2Obj.wrInIndex = 0;
    uart2Obj.wrOutIndex = 0;
    uart2Obj.isWrNotificationEnabled = false;
    uart2Obj.isWrNotifyPersistently = false;
    uart2Obj.wrThreshold = 0;

    uart2Obj.errors = UART_ERROR_NONE;

    if (UART2_IS_9BIT_MODE_ENABLED())
    {
        uart2Obj.rdBufferSize = UART2_READ_BUFFER_SIZE_9BIT;
        uart2Obj.wrBufferSize = UART2_WRITE_BUFFER_SIZE_9BIT;
    }
    else
    {
        uart2Obj.rdBufferSize = UART2_READ_BUFFER_SIZE;
        uart2Obj.wrBufferSize = UART2_WRITE_BUFFER_SIZE;
    }

    U2MODESET = _U2MODE_ON_MASK;

    IEC3SET = _IEC3_U2EIE_MASK;
    IEC3SET = _IEC3_U2RXIE_MASK;
}

bool UART2_SerialSetup(UART_SERIAL_SETUP *setup, uint32_t srcClkFreq)
{
    bool status = false;
    uint32_t baud;
    uint32_t status_ctrl;
    uint32_t uxbrg = 0;

    if (setup != NULL)
    {
        baud = setup->baudRate;

        if ((baud == 0U) || ((setup->dataWidth == UART_DATA_9_BIT) && (setup->parity != UART_PARITY_NONE)))
        {
            return status;
        }

        if (srcClkFreq == 0U)
        {
            srcClkFreq = UART2_FrequencyGet();
        }

        uxbrg = (((srcClkFreq >> 2) + (baud >> 1)) / baud);

        if (uxbrg < 1U)
        {
            return status;
        }

        uxbrg -= 1U;

        if (uxbrg > UINT16_MAX)
        {
            return status;
        }

        status_ctrl = U2STA & (_U2STA_UTXEN_MASK | _U2STA_URXEN_MASK | _U2STA_UTXBRK_MASK);

        U2MODECLR = _U2MODE_ON_MASK;

        if (setup->dataWidth == UART_DATA_9_BIT)
        {
            U2MODE = (U2MODE & (~_U2MODE_PDSEL_MASK)) | setup->dataWidth;
        }
        else
        {
            U2MODE = (U2MODE & (~_U2MODE_PDSEL_MASK)) | setup->parity;
        }

        U2MODE = (U2MODE & (~_U2MODE_STSEL_MASK)) | setup->stopBits;
        U2BRG  = uxbrg;

        if (UART2_IS_9BIT_MODE_ENABLED())
        {
            uart2Obj.rdBufferSize = UART2_READ_BUFFER_SIZE_9BIT;
            uart2Obj.wrBufferSize = UART2_WRITE_BUFFER_SIZE_9BIT;
        }
        else
        {
            uart2Obj.rdBufferSize = UART2_READ_BUFFER_SIZE;
            uart2Obj.wrBufferSize = UART2_WRITE_BUFFER_SIZE;
        }

        U2MODESET = _U2MODE_ON_MASK;
        U2STASET  = status_ctrl;

        status = true;
    }

    return status;
}

static inline bool UART2_RxPushByte(uint16_t rdByte)
{
    uint32_t tempInIndex;
    bool isSuccess = false;
    uint32_t rdInIdx;

    tempInIndex = uart2Obj.rdInIndex + 1U;

    if (tempInIndex >= uart2Obj.rdBufferSize)
    {
        tempInIndex = 0U;
    }

    if (tempInIndex == uart2Obj.rdOutIndex)
    {
        if (uart2Obj.rdCallback != NULL)
        {
            uintptr_t rdContext = uart2Obj.rdContext;
            uart2Obj.rdCallback(UART_EVENT_READ_BUFFER_FULL, rdContext);

            tempInIndex = uart2Obj.rdInIndex + 1U;
            if (tempInIndex >= uart2Obj.rdBufferSize)
            {
                tempInIndex = 0U;
            }
        }
    }

    if (tempInIndex != uart2Obj.rdOutIndex)
    {
        uint32_t rdInIndex = uart2Obj.rdInIndex;

        if (UART2_IS_9BIT_MODE_ENABLED())
        {
            rdInIdx = rdInIndex << 1U;
            UART2_ReadBuffer[rdInIdx]      = (uint8_t)rdByte;
            UART2_ReadBuffer[rdInIdx + 1U] = (uint8_t)(rdByte >> 8U);
        }
        else
        {
            UART2_ReadBuffer[rdInIndex] = (uint8_t)rdByte;
        }

        uart2Obj.rdInIndex = tempInIndex;
        isSuccess = true;
    }

    return isSuccess;
}

static void UART2_ReadNotificationSend(void)
{
    uint32_t nUnreadBytesAvailable;

    if (uart2Obj.isRdNotificationEnabled == true)
    {
        nUnreadBytesAvailable = UART2_ReadCountGet();

        if (uart2Obj.rdCallback != NULL)
        {
            uintptr_t rdContext = uart2Obj.rdContext;

            if (uart2Obj.isRdNotifyPersistently == true)
            {
                if (nUnreadBytesAvailable >= uart2Obj.rdThreshold)
                {
                    uart2Obj.rdCallback(UART_EVENT_READ_THRESHOLD_REACHED, rdContext);
                }
            }
            else
            {
                if (nUnreadBytesAvailable == uart2Obj.rdThreshold)
                {
                    uart2Obj.rdCallback(UART_EVENT_READ_THRESHOLD_REACHED, rdContext);
                }
            }
        }
    }
}

size_t UART2_Read(uint8_t* pRdBuffer, const size_t size)
{
    size_t nBytesRead = 0;
    uint32_t rdOutIndex;
    uint32_t rdInIndex;
    uint32_t rdOut16Idx;
    uint32_t nBytesRead16Idx;

    rdOutIndex = uart2Obj.rdOutIndex;
    rdInIndex  = uart2Obj.rdInIndex;

    while (nBytesRead < size)
    {
        if (rdOutIndex != rdInIndex)
        {
            if (UART2_IS_9BIT_MODE_ENABLED())
            {
                rdOut16Idx      = rdOutIndex << 1U;
                nBytesRead16Idx = nBytesRead  << 1U;
                pRdBuffer[nBytesRead16Idx]      = UART2_ReadBuffer[rdOut16Idx];
                pRdBuffer[nBytesRead16Idx + 1U] = UART2_ReadBuffer[rdOut16Idx + 1U];
            }
            else
            {
                pRdBuffer[nBytesRead] = UART2_ReadBuffer[rdOutIndex];
            }

            nBytesRead++;
            rdOutIndex++;

            if (rdOutIndex >= uart2Obj.rdBufferSize)
            {
                rdOutIndex = 0U;
            }
        }
        else
        {
            break;
        }
    }

    uart2Obj.rdOutIndex = rdOutIndex;
    return nBytesRead;
}

size_t UART2_ReadCountGet(void)
{
    uint32_t rdInIndex  = uart2Obj.rdInIndex;
    uint32_t rdOutIndex = uart2Obj.rdOutIndex;

    if (rdInIndex >= rdOutIndex)
    {
        return rdInIndex - rdOutIndex;
    }
    else
    {
        return (uart2Obj.rdBufferSize - rdOutIndex) + rdInIndex;
    }
}

size_t UART2_ReadFreeBufferCountGet(void)
{
    return (uart2Obj.rdBufferSize - 1U) - UART2_ReadCountGet();
}

size_t UART2_ReadBufferSizeGet(void)
{
    return (uart2Obj.rdBufferSize - 1U);
}

bool UART2_ReadNotificationEnable(bool isEnabled, bool isPersistent)
{
    bool previousStatus = uart2Obj.isRdNotificationEnabled;
    uart2Obj.isRdNotificationEnabled = isEnabled;
    uart2Obj.isRdNotifyPersistently  = isPersistent;
    return previousStatus;
}

void UART2_ReadThresholdSet(uint32_t nBytesThreshold)
{
    if (nBytesThreshold > 0U)
    {
        uart2Obj.rdThreshold = nBytesThreshold;
    }
}

void UART2_ReadCallbackRegister(UART_RING_BUFFER_CALLBACK callback, uintptr_t context)
{
    uart2Obj.rdCallback = callback;
    uart2Obj.rdContext  = context;
}

static bool UART2_TxPullByte(uint16_t* pWrByte)
{
    bool isSuccess = false;
    uint32_t wrOutIndex = uart2Obj.wrOutIndex;
    uint32_t wrInIndex  = uart2Obj.wrInIndex;
    uint32_t wrOut16Idx;

    if (wrOutIndex != wrInIndex)
    {
        if (UART2_IS_9BIT_MODE_ENABLED())
        {
            wrOut16Idx  = wrOutIndex << 1U;
            pWrByte[0]  = UART2_WriteBuffer[wrOut16Idx];
            pWrByte[1]  = UART2_WriteBuffer[wrOut16Idx + 1U];
        }
        else
        {
            *pWrByte = UART2_WriteBuffer[wrOutIndex];
        }

        wrOutIndex++;
        if (wrOutIndex >= uart2Obj.wrBufferSize)
        {
            wrOutIndex = 0U;
        }

        uart2Obj.wrOutIndex = wrOutIndex;
        isSuccess = true;
    }

    return isSuccess;
}

static inline bool UART2_TxPushByte(uint16_t wrByte)
{
    uint32_t tempInIndex;
    bool isSuccess = false;
    uint32_t wrOutIndex = uart2Obj.wrOutIndex;
    uint32_t wrInIndex  = uart2Obj.wrInIndex;
    uint32_t wrIn16Idx;

    tempInIndex = wrInIndex + 1U;
    if (tempInIndex >= uart2Obj.wrBufferSize)
    {
        tempInIndex = 0U;
    }

    if (tempInIndex != wrOutIndex)
    {
        if (UART2_IS_9BIT_MODE_ENABLED())
        {
            wrIn16Idx = wrInIndex << 1U;
            UART2_WriteBuffer[wrIn16Idx]      = (uint8_t)wrByte;
            UART2_WriteBuffer[wrIn16Idx + 1U] = (uint8_t)(wrByte >> 8U);
        }
        else
        {
            UART2_WriteBuffer[wrInIndex] = (uint8_t)wrByte;
        }

        uart2Obj.wrInIndex = tempInIndex;
        isSuccess = true;
    }

    return isSuccess;
}

static void UART2_WriteNotificationSend(void)
{
    uint32_t nFreeWrBufferCount;

    if (uart2Obj.isWrNotificationEnabled == true)
    {
        nFreeWrBufferCount = UART2_WriteFreeBufferCountGet();

        if (uart2Obj.wrCallback != NULL)
        {
            uintptr_t wrContext = uart2Obj.wrContext;

            if (uart2Obj.isWrNotifyPersistently == true)
            {
                if (nFreeWrBufferCount >= uart2Obj.wrThreshold)
                {
                    uart2Obj.wrCallback(UART_EVENT_WRITE_THRESHOLD_REACHED, wrContext);
                }
            }
            else
            {
                if (nFreeWrBufferCount == uart2Obj.wrThreshold)
                {
                    uart2Obj.wrCallback(UART_EVENT_WRITE_THRESHOLD_REACHED, wrContext);
                }
            }
        }
    }
}

static size_t UART2_WritePendingBytesGet(void)
{
    uint32_t wrInIndex  = uart2Obj.wrInIndex;
    uint32_t wrOutIndex = uart2Obj.wrOutIndex;

    if (wrInIndex >= wrOutIndex)
    {
        return wrInIndex - wrOutIndex;
    }
    else
    {
        return (uart2Obj.wrBufferSize - wrOutIndex) + wrInIndex;
    }
}

size_t UART2_WriteCountGet(void)
{
    return UART2_WritePendingBytesGet();
}

size_t UART2_Write(uint8_t* pWrBuffer, const size_t size)
{
    size_t nBytesWritten = 0;
    uint16_t halfWordData = 0U;

    while (nBytesWritten < size)
    {
        if (UART2_IS_9BIT_MODE_ENABLED())
        {
            halfWordData  = pWrBuffer[(2U * nBytesWritten) + 1U];
            halfWordData <<= 8U;
            halfWordData  |= pWrBuffer[(2U * nBytesWritten)];
            if (UART2_TxPushByte(halfWordData) == true)
            {
                nBytesWritten++;
            }
            else
            {
                break;
            }
        }
        else
        {
            if (UART2_TxPushByte(pWrBuffer[nBytesWritten]) == true)
            {
                nBytesWritten++;
            }
            else
            {
                break;
            }
        }
    }

    if (UART2_WritePendingBytesGet() > 0U)
    {
        UART2_TX_INT_ENABLE();
    }

    return nBytesWritten;
}

size_t UART2_WriteFreeBufferCountGet(void)
{
    return (uart2Obj.wrBufferSize - 1U) - UART2_WriteCountGet();
}

size_t UART2_WriteBufferSizeGet(void)
{
    return (uart2Obj.wrBufferSize - 1U);
}

bool UART2_TransmitComplete(void)
{
    return ((U2STA & _U2STA_TRMT_MASK) != 0U);
}

bool UART2_WriteNotificationEnable(bool isEnabled, bool isPersistent)
{
    bool previousStatus = uart2Obj.isWrNotificationEnabled;
    uart2Obj.isWrNotificationEnabled = isEnabled;
    uart2Obj.isWrNotifyPersistently  = isPersistent;
    return previousStatus;
}

void UART2_WriteThresholdSet(uint32_t nBytesThreshold)
{
    if (nBytesThreshold > 0U)
    {
        uart2Obj.wrThreshold = nBytesThreshold;
    }
}

void UART2_WriteCallbackRegister(UART_RING_BUFFER_CALLBACK callback, uintptr_t context)
{
    uart2Obj.wrCallback = callback;
    uart2Obj.wrContext  = context;
}

UART_ERROR UART2_ErrorGet(void)
{
    UART_ERROR errors = uart2Obj.errors;
    uart2Obj.errors = UART_ERROR_NONE;
    return errors;
}

bool UART2_AutoBaudQuery(void)
{
    return ((U2MODE & _U2MODE_ABAUD_MASK) != 0U);
}

void UART2_AutoBaudSet(bool enable)
{
    if (enable == true)
    {
        U2MODESET = _U2MODE_ABAUD_MASK;
    }
}

void __attribute__((used)) UART2_FAULT_InterruptHandler(void)
{
    uart2Obj.errors = (UART_ERROR)(U2STA & (_U2STA_OERR_MASK | _U2STA_FERR_MASK | _U2STA_PERR_MASK));

    UART2_ErrorClear();

    if (uart2Obj.rdCallback != NULL)
    {
        uintptr_t rdContext = uart2Obj.rdContext;
        uart2Obj.rdCallback(UART_EVENT_READ_ERROR, rdContext);
    }
}

void __attribute__((used)) UART2_RX_InterruptHandler(void)
{
    while ((U2STA & _U2STA_URXDA_MASK) == _U2STA_URXDA_MASK)
    {
        if (UART2_RxPushByte((uint16_t)(U2RXREG)) == true)
        {
            UART2_ReadNotificationSend();
        }
    }

    IFS3CLR = _IFS3_U2RXIF_MASK;
}

void __attribute__((used)) UART2_TX_InterruptHandler(void)
{
    uint16_t wrByte;

    if (UART2_WritePendingBytesGet() > 0U)
    {
        while ((U2STA & _U2STA_UTXBF_MASK) == 0U)
        {
            if (UART2_TxPullByte(&wrByte) == true)
            {
                if (UART2_IS_9BIT_MODE_ENABLED())
                {
                    U2TXREG = wrByte;
                }
                else
                {
                    U2TXREG = (uint8_t)wrByte;
                }

                UART2_WriteNotificationSend();
            }
            else
            {
                UART2_TX_INT_DISABLE();
                break;
            }
        }

        IFS3CLR = _IFS3_U2TXIF_MASK;
    }
    else
    {
        UART2_TX_INT_DISABLE();
    }
}
