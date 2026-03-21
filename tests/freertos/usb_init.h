/*
 * usb_init.h — USB CDC initialization and TX API for PIC32MK QEMU test.
 *
 * Wraps the Harmony 3 USB device + CDC layers.  Call USB1_Initialize()
 * once from main() before starting the scheduler, then create the
 * vUsbDeviceTask FreeRTOS task.  Use USB_SendFrame() to transmit data.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef USB_INIT_H
#define USB_INIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * USB1_Initialize — initialise the Harmony USB driver + device layer.
 * Must be called before vTaskStartScheduler().
 */
void USB1_Initialize(void);

/*
 * USB1_InterruptHandler — called from vUSB1InterruptWrapper (ISR context).
 * Delegates to DRV_USBFS_Tasks_ISR.
 */
void USB1_InterruptHandler(void);

/*
 * USB_SendFrame — queue up to 64 bytes for CDC TX.
 * Returns true if the write was accepted, false if busy / not configured.
 * Non-blocking: does NOT wait for the transfer to complete.
 */
bool USB_SendFrame(const uint8_t *data, size_t len);

/*
 * vUsbDeviceTask — FreeRTOS task: calls USB_DEVICE_Tasks() every 10 ms.
 * Stack: 512 words minimum.
 */
void vUsbDeviceTask(void *pvParam);

#endif /* USB_INIT_H */
