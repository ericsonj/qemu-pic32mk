/*
 * main.c — FreeRTOS Hello World on PIC32MK QEMU
 *
 * Creates one task that prints "Hello from FreeRTOS!\r\n" every second.
 * Timer1 drives the FreeRTOS tick at 1 kHz (the default port configuration).
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef snprintf
#undef snprintf
#endif

#ifdef vsnprintf
#undef vsnprintf
#endif
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "xc.h"
#include "plib_evic.h"
#include "plib_uart1.h"
#include "plib_wdt.h"
#include "plib_uart2.h"
#include "plib_canfd1.h"
#include "plib_canfd2.h"
#include "plib_clk.h"
#include "usb_init.h"
#include "plib_gpio.h"
#include "plib_adchs.h"
#include "plib_eeprom.h"
#include "plib_nvm.h"
#include "plib_ocmp1.h"
#include "plib_ocmp2.h"
#include "plib_ocmp3.h"
#include "plib_icap2.h"
#include "plib_spi5_slave.h"

/* -----------------------------------------------------------------------
 * UART1 TX helpers — thin wrappers around plib UART1_Write()
 * ----------------------------------------------------------------------- */

static void uart1_putc(char c)
{
    uint8_t byte = (uint8_t)c;
    while (UART1_WriteFreeBufferCountGet() == 0U)
    { /* spin */
    }
    UART1_Write(&byte, 1);
}

static void uart1_write_all(const char *buf, size_t len)
{
    while (len > 0U)
    {
        size_t n = UART1_Write((uint8_t *)(uintptr_t)buf, len);
        buf += n;
        len -= n;
    }
}

static void uart1_puts(const char *s)
{
    uart1_write_all(s, strlen(s));
}

int printf(const char *format, ...)
{
    char buf[192];
    int written;
    size_t len;
    va_list args;

    va_start(args, format);
    written = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (written < 0)
    {
        return written;
    }

    len = (size_t)written;
    if (len >= sizeof(buf))
    {
        len = sizeof(buf) - 1U;
    }

    uart1_write_all(buf, len);
    return written;
}


/* -----------------------------------------------------------------------
 * FreeRTOS application hooks
 * ----------------------------------------------------------------------- */

void vApplicationMallocFailedHook(void)
{
    uart1_puts("FATAL: malloc failed\r\n");
    for (;;)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    uart1_puts("FATAL: stack overflow\r\n");
    for (;;)
    {
    }
}

void vApplicationIdleHook(void)
{
    /* Kick the watchdog each idle cycle to prevent timeout. */
    WDT_Clear();
    /* Halt the CPU until the next interrupt (e.g. Timer1 tick).
     * This lets QEMU's TCG thread sleep instead of busy-looping. */
    __asm__ __volatile__("wait");
}

/* -----------------------------------------------------------------------
 * Hello World task
 * ----------------------------------------------------------------------- */

static void vHelloTask(void *pvParam)
{
    (void)pvParam;
    for (;;)
    {
        uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t s = ms / 1000;
        uint32_t m = s / 60;
        s %= 60;
        uint32_t h = m / 60;
        m %= 60;
        printf("[%02lu:%02lu:%02lu] Hello from FreeRTOS!\r\n",
               (unsigned long)h,
               (unsigned long)m,
               (unsigned long)s);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void vPingTask(void *pvParam)
{
    (void)pvParam;
    for (;;)
    {
        uart1_puts("Ping\r\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* -----------------------------------------------------------------------
 * CAN TX task — sends a 5-byte "hello" frame on CAN1 TX Queue every 3s
 * ----------------------------------------------------------------------- */

static void vCanTxTask(void *pvParam)
{
    (void)pvParam;
    static const uint8_t hello_payload[] = "hello";
    uint32_t count = 0;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        count++;
         printf("[CAN] TX #%lu id=0x100 data='hello'\r\n",
             (unsigned long)count);
        bool ok = CAN1_MessageTransmit(
            0x100U, 5U, (uint8_t *)(uintptr_t)hello_payload,
            0U,                /* TX Queue (fifoQueueNum=0) */
            CANFD_MODE_NORMAL, /* classic CAN, no BRS */
            CANFD_MSG_TX_DATA_FRAME);
        if (!ok)
        {
            uart1_puts("[CAN] TX queue full\r\n");
        }
    }
}

/* -----------------------------------------------------------------------
 * CAN2 RX task — interrupt-driven receive via FreeRTOS queue
 *
 * plib_canfd2 uses FIFO2 for RX.  can2_rx_callback() is called from
 * CAN2_InterruptHandler (ISR context) after the plib fills the static
 * receive buffer.  We copy the frame into the queue and re-arm the plib.
 * ----------------------------------------------------------------------- */

typedef struct
{
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} CAN2Frame_t;

/* -----------------------------------------------------------------------
 * CAN1 RX task — interrupt-driven receive via FreeRTOS queue
 *
 * plib_canfd1 uses FIFO2 for RX.  can1_rx_callback() is called from
 * CAN1_InterruptHandler (ISR context) after the plib fills the static
 * receive buffer.  We copy the frame into the queue and re-arm the plib.
 * ----------------------------------------------------------------------- */

typedef struct
{
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} CAN1Frame_t;

static QueueHandle_t xCan1RxQueue;

static uint32_t can1_rx_id;
static uint8_t can1_rx_len;
static uint8_t can1_rx_data[8];
static uint32_t can1_rx_ts;
static CANFD_MSG_RX_ATTRIBUTE can1_rx_attr;

static void can1_rx_callback(uintptr_t context)
{
    (void)context;
    CAN1Frame_t frame;
    uint8_t i;
    frame.id = can1_rx_id;
    frame.length = can1_rx_len;
    for (i = 0; i < can1_rx_len && i < 8U; i++)
    {
        frame.data[i] = can1_rx_data[i];
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(xCan1RxQueue, &frame, &xHigherPriorityTaskWoken);
    /* Re-arm: register buffer for the next incoming frame */
    CAN1_MessageReceive(&can1_rx_id, &can1_rx_len, can1_rx_data,
                        &can1_rx_ts, 2U, &can1_rx_attr);
    /* Yield handled by portRESTORE_CONTEXT in vCAN1InterruptWrapper */
}

static void vCan1RxTask(void *pvParam)
{
    (void)pvParam;
    CAN1Frame_t frame;
    for (;;)
    {
        if (xQueueReceive(xCan1RxQueue, &frame, portMAX_DELAY) == pdTRUE)
        {
            uint8_t i;
            printf("[CAN1 RX] id=0x%08lX len=%u data=",
                   (unsigned long)frame.id,
                   (unsigned int)frame.length);
            for (i = 0; i < frame.length && i < 8U; i++)
            {
                printf("%02X", (unsigned int)frame.data[i]);
            }
            printf("\r\n");
        }
    }
}

static QueueHandle_t xCan2RxQueue;

static uint32_t can2_rx_id;
static uint8_t can2_rx_len;
static uint8_t can2_rx_data[8];
static uint32_t can2_rx_ts;
static CANFD_MSG_RX_ATTRIBUTE can2_rx_attr;

static void can2_rx_callback(uintptr_t context)
{
    (void)context;
    CAN2Frame_t frame;
    uint8_t i;
    frame.id = can2_rx_id;
    frame.length = can2_rx_len;
    for (i = 0; i < can2_rx_len && i < 8U; i++)
    {
        frame.data[i] = can2_rx_data[i];
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(xCan2RxQueue, &frame, &xHigherPriorityTaskWoken);
    /* Re-arm: register buffer for the next incoming frame */
    CAN2_MessageReceive(&can2_rx_id, &can2_rx_len, can2_rx_data,
                        &can2_rx_ts, 2U, &can2_rx_attr);
    /* Yield handled by portRESTORE_CONTEXT in vCAN2InterruptWrapper */
}

static void vCan2RxTask(void *pvParam)
{
    (void)pvParam;
    CAN2Frame_t frame;
    for (;;)
    {
        if (xQueueReceive(xCan2RxQueue, &frame, portMAX_DELAY) == pdTRUE)
        {
            uint8_t i;
            printf("[CAN2 RX] id=0x%lX len=%u data='",
                   (unsigned long)frame.id,
                   (unsigned int)frame.length);
            for (i = 0; i < frame.length && i < 8U; i++)
            {
                uart1_putc((char)frame.data[i]);
            }
            printf("'\r\n");
        }
    }
}

/* -----------------------------------------------------------------------
 * UART2 RX queue and callback
 *
 * xUart2RxQueue holds individual bytes received on UART2.
 * uart2_rx_callback() is called from UART2_RX_InterruptHandler (ISR
 * context) whenever the plib RX threshold (1 byte) is reached.
 * portRESTORE_CONTEXT in the ISR wrapper always calls vTaskSwitchContext,
 * so the consumer task is woken up on the next scheduler pass.
 * ----------------------------------------------------------------------- */

static QueueHandle_t xUart2RxQueue;

static void uart2_rx_callback(UART_EVENT event, uintptr_t context)
{
    (void)context;
    if (event != UART_EVENT_READ_THRESHOLD_REACHED)
    {
        return;
    }

    uint8_t byte;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (UART2_Read(&byte, 1) == 1)
    {
        xQueueSendToBackFromISR(xUart2RxQueue, &byte, &xHigherPriorityTaskWoken);
    }
    /* Yield is handled by portRESTORE_CONTEXT in vUART2RXInterruptWrapper */
}

/* -----------------------------------------------------------------------
 * UART2 consumer task
 *
 * Blocks on the queue until a byte arrives, then echoes it on UART1
 * as: "[U2] <hex> '<ch>'\r\n"
 * ----------------------------------------------------------------------- */

static void vUart2ConsumerTask(void *pvParam)
{
    (void)pvParam;
    for (;;)
    {
        uint8_t byte;
        if (xQueueReceive(xUart2RxQueue, &byte, portMAX_DELAY) == pdTRUE)
        {
            printf("[U2] 0x%02X '%c'\r\n",
                   (unsigned int)byte,
                   (char)byte);
        }
    }
}

/* -----------------------------------------------------------------------
 * USB CDC TX task
 *
 * Waits 3 s for USB enumeration, then sends "[CDC] Hello #N\r\n" every
 * second.  Uses USB_SendFrame() which is non-blocking.
 * ----------------------------------------------------------------------- */

static void vUsbCdcTask(void *pvParam)
{
    (void)pvParam;
    static uint8_t s_buf[48];
    uint32_t count = 0;

    /* Wait for host to enumerate the device */
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (;;)
    {
        int len;

        count++;

        len = snprintf((char *)s_buf, sizeof(s_buf),
                       "[CDC] Hello #%lu\r\n",
                       (unsigned long)count);

        /* Retry until accepted (USB may still be busy from previous TX) */
        if (len > 0)
        {
            size_t tx_len = (size_t)len;
            if (tx_len >= sizeof(s_buf))
            {
                tx_len = sizeof(s_buf) - 1U;
            }
            while (!USB_SendFrame(s_buf, tx_len))
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -----------------------------------------------------------------------
 * GPIO Port A tasks
 *
 * Pin assignment from plib_gpio.h (VOLTU hardware):
 *   RA15 (GPO_GND_DIFF_LOCK) — digital output, configured by GPIO_Initialize()
 *   RA7  (SPI_EN)            — digital input with CN, configured by GPIO_Initialize()
 *
 * vGpioBlinkTask   — toggles RA15 every 500 ms
 * vGpioMonitorTask — waits for CN interrupt on RA7, prints state change
 *
 * Host-side workflow (requires QEMU launched with -qmp socket):
 *   python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock set A 7 1
 *   python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock set A 7 0
 *   python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock get-port A
 * ----------------------------------------------------------------------- */

static SemaphoreHandle_t xGpioCnSemaphore;

static void gpio_ra7_callback(GPIO_PIN pin, uintptr_t context)
{
    (void)pin;
    (void)context;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xGpioCnSemaphore, &xHigherPriorityTaskWoken);
}

static void vGpioBlinkTask(void *pvParam)
{
    (void)pvParam;

    /* RA15 (GPO_GND_DIFF_LOCK): configured as digital output by GPIO_Initialize()
     * (ANSELACLR = 0xd803 clears bit 15, TRISACLR = 0xc401 clears bit 15) */
    uart1_puts("[GPIO] RA15 blink started (500ms toggle)\r\n");

    for (;;)
    {
        GPO_GND_DIFF_LOCK_Toggle();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void vGpioMonitorTask(void *pvParam)
{
    (void)pvParam;
    uint8_t prev_ra7 = 0;

    /* RA7 (SPI_EN): input-only pin on port A.
     * GPIO_Initialize() already enabled CNCON.ON and IEC1.CNAIE.
     * We just register the callback and enable per-pin CN detection. */
    xGpioCnSemaphore = xSemaphoreCreateBinary();
    GPIO_PinInterruptCallbackRegister(SPI_EN_PIN, gpio_ra7_callback, 0);
    GPIO_PinIntEnable(SPI_EN_PIN, GPIO_INTERRUPT_ON_BOTH_EDGES);

    uart1_puts("[GPIO] RA7 (SPI_EN) CN interrupt monitor started\r\n");
    uart1_puts("[GPIO] Inject: gpio_tool.py set A 7 1\r\n");

    for (;;)
    {
        xSemaphoreTake(xGpioCnSemaphore, portMAX_DELAY);

        uint8_t ra7 = (uint8_t)SPI_EN_Get();

        if (ra7 != prev_ra7)
        {
            if (ra7)
            {
                uart1_puts("[GPIO] RA7: LOW -> HIGH (rising edge)\r\n");
            }
            else
            {
                uart1_puts("[GPIO] RA7: HIGH -> LOW (falling edge)\r\n");
            }
            prev_ra7 = ra7;
        }
    }
}

/* -----------------------------------------------------------------------
 * ADC Monitor task — reads CH15 (TEMP_MOTOR) via EOS interrupt
 * ----------------------------------------------------------------------- */

static SemaphoreHandle_t xAdcEosSemaphore;

static void adc_eos_callback(uintptr_t context)
{
    (void)context;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xAdcEosSemaphore, &xHigherPriorityTaskWoken);
}

static void vAdcMonitorTask(void *pvParam)
{
    (void)pvParam;

    xAdcEosSemaphore = xSemaphoreCreateBinary();
    ADCHS_EOSCallbackRegister(adc_eos_callback, 0);

    uart1_puts("[ADC] CH15 (TEMP_MOTOR) monitor started\r\n");
    uart1_puts("[ADC] Inject: gpio_tool.py adc-set 15 2048\r\n");

    for (;;)
    {
        /* Trigger a global software edge conversion */
        ADCHS_GlobalEdgeConversionStart();

        /* Wait for End-of-Scan interrupt */
        xSemaphoreTake(xAdcEosSemaphore, portMAX_DELAY);

        /* Read channel 15 result */
        if (ADCHS_ChannelResultIsReady(ADCHS_CH15))
        {
            uint32_t result = ADCHS_ChannelResultGet(ADCHS_CH15);
            printf("[ADC] CH15 = %lu\r\n", (unsigned long)result);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* -----------------------------------------------------------------------
 * EEPROM Test task — exercises Data EEPROM via Harmony plib
 *
 * Sequence: init -> write 4 words -> read back & verify -> page erase ->
 *           verify erased -> bulk erase -> done.
 * Prints PASS/FAIL for each step.
 * ----------------------------------------------------------------------- */

static void vEepromTestTask(void *pvParam)
{
    (void)pvParam;

    uart1_puts("[EE] EEPROM test starting...\r\n");

    /* --- Write 4 words at addresses 0x000, 0x004, 0x008, 0x00C --- */
    static const uint32_t test_addr[] = { 0x000, 0x004, 0x008, 0x00C };
    static const uint32_t test_data[] = { 0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xA5A5A5A5 };
    uint32_t i;
    bool ok;

    for (i = 0; i < 4; i++) {
        ok = EEPROM_WordWrite(test_addr[i], test_data[i]);
        if (!ok) {
            printf("[EE] FAIL: WordWrite addr=0x%08lX\r\n",
                   (unsigned long)test_addr[i]);
        }
    }

    EEPROM_ERROR err = EEPROM_ErrorGet();
    if (err != EEPROM_ERROR_NONE) {
        printf("[EE] FAIL: ErrorGet after writes = 0x%08lX\r\n",
               (unsigned long)err);
    }

    /* --- Read back and verify --- */
    uint32_t pass_count = 0;
    for (i = 0; i < 4; i++) {
        uint32_t readback = 0;
        ok = EEPROM_WordRead(test_addr[i], &readback);
        if (!ok) {
            printf("[EE] FAIL: WordRead addr=0x%08lX\r\n",
                   (unsigned long)test_addr[i]);
        } else if (readback != test_data[i]) {
            printf("[EE] FAIL: addr=0x%08lX expected=0x%08lX got=0x%08lX\r\n",
                   (unsigned long)test_addr[i],
                   (unsigned long)test_data[i],
                   (unsigned long)readback);
        } else {
            pass_count++;
        }
    }
    printf("[EE] Write/Read: %lu/4 PASS\r\n", (unsigned long)pass_count);


    /* --- Page erase (page 0 = addresses 0x000..0x07C) --- */
    ok = EEPROM_PageErase(0x000);
    if (!ok) {
        uart1_puts("[EE] FAIL: PageErase\r\n");
    }
    /* Verify erased (should read 0xFFFFFFFF) */
    pass_count = 0;
    for (i = 0; i < 4; i++) {
        uint32_t readback = 0;
        ok = EEPROM_WordRead(test_addr[i], &readback);
        if (ok && readback == 0xFFFFFFFFu) {
            pass_count++;
        } else {
            printf("[EE] FAIL: post-erase addr=0x%08lX got=0x%08lX\r\n",
                   (unsigned long)test_addr[i],
                   (unsigned long)readback);
        }
    }
    printf("[EE] PageErase: %lu/4 PASS\r\n", (unsigned long)pass_count);

    /* --- Bulk erase test: write one word, bulk erase, verify --- */
    EEPROM_WordWrite(0x100, 0xBAADF00Du);
    ok = EEPROM_BulkErase();
    if (!ok) {
        uart1_puts("[EE] FAIL: BulkErase\r\n");
    }
    {
        uint32_t readback = 0;
        EEPROM_WordRead(0x100, &readback);
        if (readback == 0xFFFFFFFFu) {
            uart1_puts("[EE] BulkErase: PASS\r\n");
        } else {
            printf("[EE] BulkErase: FAIL got=0x%08lX\r\n",
                   (unsigned long)readback);
        }
    }

    uart1_puts("[EE] EEPROM test complete.\r\n");

    /* Task done — suspend forever */
    vTaskSuspend(NULL);
}

/* -----------------------------------------------------------------------
 * NVM (Program Flash) Test task — uses Harmony plib API
 *
 * Exercises the QEMU NVM controller via the unmodified Harmony plib
 * (NVM_WordWrite, NVM_QuadWordWrite, NVM_PageErase, NVM_Read).
 * The plib enables IEC0[31] after each operation; the NVM ISR
 * (NVM_InterruptHandler) clears IFS0[31] so FreeRTOS keeps running.
 *
 * Test sequence: word write ×4 -> read back -> quad-word write ->
 *                read back -> page erase -> verify erased.
 * Uses the last page of program flash (NVM_FLASH_START_ADDRESS + 0xFF000).
 * ----------------------------------------------------------------------- */

static void vNvmTestTask(void *pvParam)
{
    (void)pvParam;

    const uint32_t test_base = NVM_FLASH_START_ADDRESS + 0xFF000u;

    uart1_puts("[NVM] NVM test starting...\r\n");

    /* --- Word write ×4 --- */
    static const uint32_t test_data[] = {
        0xDEADBEEFu, 0xCAFEBABEu, 0x12345678u, 0xA5A5A5A5u
    };
    uint32_t i;

    for (i = 0; i < 4; i++) {
        NVM_WordWrite(test_data[i], test_base + i * 4u);
        while (NVM_IsBusy()) { }
    }

    if (NVM_ErrorGet() != NVM_ERROR_NONE) {
        printf("[NVM] FAIL: error after writes = 0x%08lX\r\n",
               (unsigned long)NVM_ErrorGet());
    }

    /* --- Read back via NVM_Read and verify --- */
    uint32_t rbuf[4];
    NVM_Read(rbuf, sizeof(rbuf), test_base);

    uint32_t pass_count = 0;
    for (i = 0; i < 4; i++) {
        if (rbuf[i] != test_data[i]) {
            printf("[NVM] FAIL: word[%lu] expected=0x%08lX got=0x%08lX\r\n",
                   (unsigned long)i,
                   (unsigned long)test_data[i],
                   (unsigned long)rbuf[i]);
        } else {
            pass_count++;
        }
    }
    printf("[NVM] Write/Read: %lu/4 PASS\r\n", (unsigned long)pass_count);

    /* --- Quad-word write at offset 0x10 (16-byte aligned) --- */
    {
        static const uint32_t qw_data[4] = {
            0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u
        };
        NVM_QuadWordWrite((uint32_t *)qw_data, test_base + 0x10u);
        while (NVM_IsBusy()) { }

        uint32_t qbuf[4];
        NVM_Read(qbuf, sizeof(qbuf), test_base + 0x10u);

        if (qbuf[0] == qw_data[0] && qbuf[1] == qw_data[1] &&
            qbuf[2] == qw_data[2] && qbuf[3] == qw_data[3]) {
            uart1_puts("[NVM] QuadWord: PASS\r\n");
        } else {
            printf("[NVM] QuadWord: FAIL got=0x%08lX 0x%08lX 0x%08lX 0x%08lX\r\n",
                   (unsigned long)qbuf[0],
                   (unsigned long)qbuf[1],
                   (unsigned long)qbuf[2],
                   (unsigned long)qbuf[3]);
        }
    }

    /* --- Page erase (erases 4 KB at test_base) --- */
    NVM_PageErase(test_base);
    while (NVM_IsBusy()) { }

    /* Verify erased (should read 0xFFFFFFFF) */
    NVM_Read(rbuf, sizeof(rbuf), test_base);

    pass_count = 0;
    for (i = 0; i < 4; i++) {
        if (rbuf[i] == 0xFFFFFFFFu) {
            pass_count++;
        } else {
            printf("[NVM] FAIL: post-erase word[%lu] got=0x%08lX\r\n",
                   (unsigned long)i,
                   (unsigned long)rbuf[i]);
        }
    }
    printf("[NVM] PageErase: %lu/4 PASS\r\n", (unsigned long)pass_count);

    uart1_puts("[NVM] NVM test complete.\r\n");

    /* Task done — suspend forever */
    vTaskSuspend(NULL);
}

/* -----------------------------------------------------------------------
 * Output Compare demo task — exercises OC1 (PWM), OC2 (single pulse),
 * OC3 (continuous pulses) via Harmony OCMP plib API.
 * ----------------------------------------------------------------------- */

static void vOcDemoTask(void *pvParam)
{
    (void)pvParam;
    vTaskDelay(pdMS_TO_TICKS(2000));  /* Let other init messages print first */

    uart1_puts("[OC] Output Compare demo starting...\r\n");

    /* --- OC1: PWM mode (OCM=110, default from OCMP1_Initialize) --- */
    OCMP1_Initialize();                      /* Sets OC1CON=0x6 (PWM no fault) */
    OCMP1_CompareSecondaryValueSet(0x7FFF);  /* 50% duty: OCxRS = PR2/2 = 0xFFFF/2 */
    OCMP1_Enable();                          /* OC1CONSET = ON */
    uart1_puts("[OC] OC1: PWM mode (plib), duty=50%\r\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- OC2: Single pulse (OCM=100) ---
     * OCMP2_Initialize() sets OCM=6 (PWM) by default, so we override
     * to mode 4 (single pulse) via direct register write after init. */
    OCMP2_Initialize();
    OC2CON = 0x0004;                          /* OCM=100 (single pulse) */
    OC2R   = 0x0100;                          /* Primary compare */
    OCMP2_CompareSecondaryValueSet(0x0500);   /* Secondary compare */
    OCMP2_Enable();
    uart1_puts("[OC] OC2: Single pulse mode, R=0x100 RS=0x500\r\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- OC3: Continuous pulses (OCM=101) --- */
    OCMP3_Initialize();
    OC3CON = 0x0005;                          /* OCM=101 (continuous pulses) */
    OC3R   = 0x0200;
    OCMP3_CompareSecondaryValueSet(0x0800);
    OCMP3_Enable();
    uart1_puts("[OC] OC3: Continuous pulse mode\r\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Disable all via plib --- */
    OCMP1_Disable();
    OCMP2_Disable();
    OCMP3_Disable();
    uart1_puts("[OC] All OC modules disabled\r\n");
    uart1_puts("[OC] Output Compare demo complete.\r\n");

    vTaskSuspend(NULL);
}

/* -----------------------------------------------------------------------
 * Input Capture demo task — exercises IC2 via Harmony ICAP2 plib
 *
 * ICAP2_Initialize() enables IEC0 IC2IE + IC2EIE; the capture ISR
 * (INPUT_CAPTURE_2_InterruptHandler) calls the registered callback which
 * signals this task via semaphore.  Inject captures using the QEMU
 * ic-events chardev: write 8-byte packets as described in pic32mk_ic.c.
 * ----------------------------------------------------------------------- */

static SemaphoreHandle_t xIc2Semaphore;
static volatile uint16_t ic2_last_capture;

static void ic2_capture_callback(uintptr_t context)
{
    (void)context;
    ic2_last_capture = ICAP2_CaptureBufferRead();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xIc2Semaphore, &xHigherPriorityTaskWoken);
}

static void vIcapDemoTask(void *pvParam)
{
    (void)pvParam;

    xIc2Semaphore = xSemaphoreCreateBinary();

    ICAP2_CallbackRegister(ic2_capture_callback, 0);
    ICAP2_Initialize();   /* IC2CON = 0x1, enables IEC0 IC2IE + IC2EIE */
    ICAP2_Enable();       /* IC2CONSET = ON */

    uart1_puts("[ICAP] IC2 armed (IRQ mode, ICM=1 every edge)\r\n");

    for (;;)
    {
        xSemaphoreTake(xIc2Semaphore, portMAX_DELAY);
        uint16_t val = ic2_last_capture;
        printf("[ICAP] IC2 captured 0x%04X\r\n", (unsigned int)val);
    }
}

/* -----------------------------------------------------------------------
 * SPI5 Slave demo — receive HELLO from host and reply with WORLD
 * ----------------------------------------------------------------------- */

static void vSpi5SlaveTask(void *pvParam)
{
    (void)pvParam;
    static const uint8_t reply[] = "WORLD";
    uint8_t buf[64];

    uart1_puts("[SPI5] Slave demo ready (send HELLO over socket)\r\n");

    /* Preload the response buffer for MISO */
    (void)SPI5_Write((void *)(uintptr_t)reply, sizeof(reply) - 1U);

    for (;;)
    {
        size_t count = SPI5_ReadCountGet();
        if (count > 0U)
        {
            if (count > sizeof(buf))
            {
                count = sizeof(buf);
            }
            (void)SPI5_Read(buf, count);
            printf("[SPI5] RX '");
            for (size_t i = 0; i < count; i++)
            {
                uart1_putc((char)buf[i]);
            }
            printf("'\r\n");

            /* Refill reply buffer after each transfer */
            (void)SPI5_Write((void *)(uintptr_t)reply, sizeof(reply) - 1U);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(void)
{
    CLK_Initialize();
    EVIC_Initialize();
    UART1_Initialize();
    UART2_Initialize();
    CAN1_Initialize();
    CAN2_Initialize();
    USB1_Initialize();
    GPIO_Initialize();
    ADCHS_Initialize();
    EEPROM_Initialize();
    NVM_Initialize();
    WDT_Enable();
    SPI5_Initialize();

    uart1_puts("PIC32MK QEMU booting FreeRTOS...\r\n");
    uart1_puts("UART2 RX -> FreeRTOS queue -> consumer task\r\n");
    uart1_puts("SPI5 slave ready on chardev 'spi5'\r\n");

    /* Queue for CAN1 RX frames (depth = 16) */
    xCan1RxQueue = xQueueCreate(16, sizeof(CAN1Frame_t));
    CAN1_CallbackRegister(can1_rx_callback, 0, 2U); /* FIFO2 = RX */
    CAN1_MessageReceive(&can1_rx_id, &can1_rx_len, can1_rx_data,
                        &can1_rx_ts, 2U, &can1_rx_attr);

    /* Queue for CAN2 RX frames (depth = 16) */
    xCan2RxQueue = xQueueCreate(16, sizeof(CAN2Frame_t));
    CAN2_CallbackRegister(can2_rx_callback, 0, 2U); /* FIFO2 = RX */
    CAN2_MessageReceive(&can2_rx_id, &can2_rx_len, can2_rx_data,
                        &can2_rx_ts, 2U, &can2_rx_attr);

    /* Queue for bytes received on UART2 (depth = 64 bytes) */
    xUart2RxQueue = xQueueCreate(64, sizeof(uint8_t));

    /* Register UART2 RX callback: notify per byte, non-persistent */
    UART2_ReadCallbackRegister(uart2_rx_callback, 0);
    UART2_ReadThresholdSet(10);
    UART2_ReadNotificationEnable(true, false);

    xTaskCreate(vHelloTask, "Hello", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vPingTask, "Ping", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vUart2ConsumerTask, "U2Rx", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL); /* higher priority so it runs immediately */
    xTaskCreate(vCanTxTask, "CAN", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vCan1RxTask, "C1Rx", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vCan2RxTask, "C2Rx", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vUsbDeviceTask, "USB", 512,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vUsbCdcTask, "CDC", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vGpioBlinkTask, "Blink", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vGpioMonitorTask, "GpioMon", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vAdcMonitorTask, "ADC", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vEepromTestTask, "EE", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vNvmTestTask, "NVM", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vOcDemoTask, "OC", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vIcapDemoTask, "ICAP", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vSpi5SlaveTask, "SPI5", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    uart1_puts("ERROR: scheduler returned\r\n");
    for (;;)
    {
    }
}
