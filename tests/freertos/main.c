/*
 * main.c — FreeRTOS Hello World on PIC32MK QEMU
 *
 * Creates one task that prints "Hello from FreeRTOS!\r\n" every second.
 * Timer1 drives the FreeRTOS tick at 1 kHz (the default port configuration).
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "xc.h"
#include "plib_uart1.h"
#include "plib_uart2.h"
#include "plib_canfd1.h"
#include "plib_canfd2.h"

/* -----------------------------------------------------------------------
 * UART1 TX helpers — thin wrappers around plib UART1_Write()
 * ----------------------------------------------------------------------- */

static inline size_t uart_strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

static void uart1_putc(char c)
{
    uint8_t byte = (uint8_t)c;
    while (UART1_WriteFreeBufferCountGet() == 0U) { /* spin */ }
    UART1_Write(&byte, 1);
}

static void uart1_puts(const char *s)
{
    size_t len = uart_strlen(s);
    while (len > 0U) {
        size_t n = UART1_Write((uint8_t *)(uintptr_t)s, len);
        s   += n;
        len -= n;
    }
}

/* -----------------------------------------------------------------------
 * Override the weak vApplicationSetupTickTimerInterrupt() from port.c.
 *
 * Configures Timer1 for 1 kHz tick:
 *   Prescaler 1:8  → effective clock = 120 MHz / 8 = 15 MHz
 *   PR1 = 15 MHz / 1000 - 1 = 14999
 *
 * IPC1.T1IP set to configKERNEL_INTERRUPT_PRIORITY = 1.
 * Timer1 is EVIC source 4 → IFS0[4], IEC0[4], IPC1 bits [4:2].
 * ----------------------------------------------------------------------- */

void vApplicationSetupTickTimerInterrupt(void)
{
    /* Stop Timer1 and reset */
    T1CON = 0;
    TMR1  = 0;

    /* Period register: (120MHz / 8) / 1000 - 1 = 14999 */
    PR1 = 14999u;

    /* EVIC: set T1IP = configKERNEL_INTERRUPT_PRIORITY (= 1) in IPC1[4:2] */
    IPC1CLR = (0x7u << 2);   /* clear T1IP field */
    IPC1SET = ((uint32_t)configKERNEL_INTERRUPT_PRIORITY << 2);

    /* EVIC: clear T1IF (source 4 = bit 4 of IFS0) */
    IFS0CLR = _IFS0_T1IF_MASK;

    /* EVIC: enable T1 interrupt (bit 4 of IEC0) */
    IEC0SET = _IFS0_T1IF_MASK;

    /* T1CONSET: TCKPS = 1 (prescaler 1:8, bits [5:4]) | ON (bit 15) */
    T1CONSET = (1u << 4) | (1u << 15);
}

/* -----------------------------------------------------------------------
 * FreeRTOS application hooks
 * ----------------------------------------------------------------------- */

void vApplicationMallocFailedHook(void)
{
    uart1_puts("FATAL: malloc failed\r\n");
    for (;;) {}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    uart1_puts("FATAL: stack overflow\r\n");
    for (;;) {}
}

void vApplicationIdleHook(void)
{
    /* Halt the CPU until the next interrupt (e.g. Timer1 tick).
     * This lets QEMU's TCG thread sleep instead of busy-looping. */
    __asm__ __volatile__("wait");
}

/* -----------------------------------------------------------------------
 * Hello World task
 * ----------------------------------------------------------------------- */

static void uart1_putu(uint32_t val)
{
    char buf[11];
    int i = 0;
    if (val == 0) { uart1_putc('0'); return; }
    while (val) { buf[i++] = '0' + (val % 10); val /= 10; }
    /* Reverse and send in one UART1_Write call */
    {
        uint8_t out[11];
        int j = 0;
        while (i > 0) out[j++] = (uint8_t)buf[--i];
        size_t len = (size_t)j;
        uint8_t *p = out;
        while (len > 0U) {
            size_t n = UART1_Write(p, len);
            p   += n;
            len -= n;
        }
    }
}

static void vHelloTask(void *pvParam)
{
    (void)pvParam;
    for (;;) {
        uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t s  = ms / 1000;
        uint32_t m  = s / 60;  s %= 60;
        uint32_t h  = m / 60;  m %= 60;
        uart1_putc('[');
        if (h < 10) uart1_putc('0'); uart1_putu(h); uart1_putc(':');
        if (m < 10) uart1_putc('0'); uart1_putu(m); uart1_putc(':');
        if (s < 10) uart1_putc('0'); uart1_putu(s);
        uart1_puts("] Hello from FreeRTOS!\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void vPingTask(void *pvParam)
{
    (void)pvParam;
    for (;;) {
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
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        count++;
        uart1_puts("[CAN] TX #");
        uart1_putu(count);
        uart1_puts(" id=0x100 data='hello'\r\n");
        bool ok = CAN1_MessageTransmit(
            0x100U, 5U, (uint8_t *)(uintptr_t)hello_payload,
            0U,                       /* TX Queue (fifoQueueNum=0) */
            CANFD_MODE_NORMAL,        /* classic CAN, no BRS */
            CANFD_MSG_TX_DATA_FRAME
        );
        if (!ok) {
            uart1_puts("[CAN] TX queue full\r\n");
        }
    }
}

static void uart1_puthex(uint8_t v);   /* defined below with UART2 consumer */

/* -----------------------------------------------------------------------
 * CAN2 RX task — interrupt-driven receive via FreeRTOS queue
 *
 * plib_canfd2 uses FIFO2 for RX.  can2_rx_callback() is called from
 * CAN2_InterruptHandler (ISR context) after the plib fills the static
 * receive buffer.  We copy the frame into the queue and re-arm the plib.
 * ----------------------------------------------------------------------- */

typedef struct {
    uint32_t id;
    uint8_t  length;
    uint8_t  data[8];
} CAN2Frame_t;

/* -----------------------------------------------------------------------
 * CAN1 RX task — interrupt-driven receive via FreeRTOS queue
 *
 * plib_canfd1 uses FIFO2 for RX.  can1_rx_callback() is called from
 * CAN1_InterruptHandler (ISR context) after the plib fills the static
 * receive buffer.  We copy the frame into the queue and re-arm the plib.
 * ----------------------------------------------------------------------- */

typedef struct {
    uint32_t id;
    uint8_t  length;
    uint8_t  data[8];
} CAN1Frame_t;

static QueueHandle_t xCan1RxQueue;

static uint32_t               can1_rx_id;
static uint8_t                can1_rx_len;
static uint8_t                can1_rx_data[8];
static uint32_t               can1_rx_ts;
static CANFD_MSG_RX_ATTRIBUTE can1_rx_attr;

static void can1_rx_callback(uintptr_t context)
{
    (void)context;
    CAN1Frame_t frame;
    uint8_t i;
    frame.id     = can1_rx_id;
    frame.length = can1_rx_len;
    for (i = 0; i < can1_rx_len && i < 8U; i++) {
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
    for (;;) {
        if (xQueueReceive(xCan1RxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            uint8_t i;
            uart1_puts("[CAN1 RX] id=0x");
            uart1_puthex((uint8_t)(frame.id >> 24));
            uart1_puthex((uint8_t)(frame.id >> 16));
            uart1_puthex((uint8_t)(frame.id >> 8));
            uart1_puthex((uint8_t)(frame.id));
            uart1_puts(" len=");
            uart1_putc('0' + frame.length);
            uart1_puts(" data=");
            for (i = 0; i < frame.length && i < 8U; i++) {
                uart1_puthex(frame.data[i]);
            }
            uart1_puts("\r\n");
        }
    }
}

static QueueHandle_t xCan2RxQueue;

static uint32_t               can2_rx_id;
static uint8_t                can2_rx_len;
static uint8_t                can2_rx_data[8];
static uint32_t               can2_rx_ts;
static CANFD_MSG_RX_ATTRIBUTE can2_rx_attr;

static void can2_rx_callback(uintptr_t context)
{
    (void)context;
    CAN2Frame_t frame;
    uint8_t i;
    frame.id     = can2_rx_id;
    frame.length = can2_rx_len;
    for (i = 0; i < can2_rx_len && i < 8U; i++) {
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
    for (;;) {
        if (xQueueReceive(xCan2RxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            uint8_t i;
            uart1_puts("[CAN2 RX] id=0x");
            uart1_puthex((uint8_t)(frame.id >> 8));
            uart1_puthex((uint8_t)(frame.id));
            uart1_puts(" len=");
            uart1_putc('0' + frame.length);
            uart1_puts(" data='");
            for (i = 0; i < frame.length && i < 8U; i++) {
                uart1_putc((char)frame.data[i]);
            }
            uart1_puts("'\r\n");
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
    if (event != UART_EVENT_READ_THRESHOLD_REACHED) {
        return;
    }

    uint8_t byte;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (UART2_Read(&byte, 1) == 1) {
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

static void uart1_puthex(uint8_t v)
{
    const char hex[] = "0123456789ABCDEF";
    uart1_putc(hex[v >> 4]);
    uart1_putc(hex[v & 0xFu]);
}

static void vUart2ConsumerTask(void *pvParam)
{
    (void)pvParam;
    for (;;) {
        uint8_t byte;
        if (xQueueReceive(xUart2RxQueue, &byte, portMAX_DELAY) == pdTRUE) {
            uart1_puts("[U2] 0x");
            uart1_puthex(byte);
            uart1_puts(" '");
            uart1_putc((char)byte);
            uart1_puts("'\r\n");
        }
    }
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(void)
{
    UART1_Initialize();
    UART2_Initialize();
    CAN1_Initialize();
    CAN2_Initialize();

    uart1_puts("PIC32MK QEMU booting FreeRTOS...\r\n");
    uart1_puts("UART2 RX -> FreeRTOS queue -> consumer task\r\n");

    /* Queue for CAN1 RX frames (depth = 16) */
    xCan1RxQueue = xQueueCreate(16, sizeof(CAN1Frame_t));
    CAN1_CallbackRegister(can1_rx_callback, 0, 2U);   /* FIFO2 = RX */
    CAN1_MessageReceive(&can1_rx_id, &can1_rx_len, can1_rx_data,
                        &can1_rx_ts, 2U, &can1_rx_attr);

    /* Queue for CAN2 RX frames (depth = 16) */
    xCan2RxQueue = xQueueCreate(16, sizeof(CAN2Frame_t));
    CAN2_CallbackRegister(can2_rx_callback, 0, 2U);   /* FIFO2 = RX */
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
                NULL, tskIDLE_PRIORITY + 2, NULL);   /* higher priority so it runs immediately */
    xTaskCreate(vCanTxTask, "CAN", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vCan1RxTask, "C1Rx", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vCan2RxTask, "C2Rx", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    uart1_puts("ERROR: scheduler returned\r\n");
    for (;;) {}
}
