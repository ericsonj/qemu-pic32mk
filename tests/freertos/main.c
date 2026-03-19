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
#include "xc.h"
#include "plib_uart1.h"

/* Minimal memset — FreeRTOS_tasks.c calls this without libc */
void *memset(void *dst, int c, __SIZE_TYPE__ n)
{
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

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
 * main
 * ----------------------------------------------------------------------- */

int main(void)
{
    UART1_Initialize();

    uart1_puts("PIC32MK QEMU booting FreeRTOS...\r\n");

    xTaskCreate(vHelloTask, "Hello", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vPingTask, "Ping", configMINIMAL_STACK_SIZE,
                NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    uart1_puts("ERROR: scheduler returned\r\n");
    for (;;) {}
}
