/*
 * usb_init.c — Harmony 3 USB CDC bring-up for PIC32MK QEMU test.
 *
 * Initialises DRV_USBFS + USB_DEVICE + USB_DEVICE_CDC.
 * The endpoint table is 512-byte aligned as required by the PIC32MK
 * USB controller (BDT base must be aligned to 512 bytes).
 *
 * Interrupt wiring:
 *   USB1 = EVIC source 34 → IFS1 bit 2, IEC1 bit 2.
 *   crt0.S dispatches to vUSB1InterruptWrapper → USB1_InterruptHandler().
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "definitions.h"
#include "usb_init.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xc.h"

/* --------------------------------------------------------------------------
 * External descriptor table from usb_device_init_data.c
 * -------------------------------------------------------------------------- */
extern const USB_DEVICE_INIT usbDevInitData0;

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */

/* 512-byte aligned BDT / endpoint buffer table.
 * DRV_USBFS_ENDPOINTS_NUMBER = 3 → 3 × 2 directions × 2 ping-pong
 * = 12 entries × 8 bytes = 96 bytes; allocate full 512-byte aligned block. */
static uint8_t __attribute__((aligned(512)))
    s_endpointTable[DRV_USBFS_ENDPOINTS_NUMBER * 32];

static SYS_MODULE_OBJ      s_drv_obj  = SYS_MODULE_OBJ_INVALID;
static SYS_MODULE_OBJ      s_dev_obj  = SYS_MODULE_OBJ_INVALID;
static USB_DEVICE_HANDLE   s_dev_hdl  = USB_DEVICE_HANDLE_INVALID;

/* sysObj: referenced by DRV_USBFS_USB1_Handler() in the copied drv_usbfs.c.
 * We keep it updated so that function works correctly even if called. */
SYSTEM_OBJECTS sysObj;

static volatile bool s_cdc_configured;
static volatile bool s_cdc_tx_busy;

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static void usb_device_event_handler(USB_DEVICE_EVENT event,
                                     void *eventData,
                                     uintptr_t context);
static void usb_cdc_event_handler(USB_DEVICE_CDC_INDEX index,
                                  USB_DEVICE_CDC_EVENT event,
                                  void *pData,
                                  uintptr_t context);

/* --------------------------------------------------------------------------
 * USB Device event handler
 * -------------------------------------------------------------------------- */
static void usb_device_event_handler(USB_DEVICE_EVENT event,
                                     void *eventData,
                                     uintptr_t context)
{
    (void)context;
    switch (event) {
    case USB_DEVICE_EVENT_RESET:
    case USB_DEVICE_EVENT_DECONFIGURED:
        s_cdc_configured = false;
        s_cdc_tx_busy    = false;
        break;

    case USB_DEVICE_EVENT_CONFIGURED:
        /* Register the CDC event handler now that the device is configured */
        USB_DEVICE_CDC_EventHandlerSet(USB_DEVICE_CDC_INDEX_0,
                                       usb_cdc_event_handler, 0);
        s_cdc_configured = true;
        break;

    case USB_DEVICE_EVENT_POWER_DETECTED:
        USB_DEVICE_Attach(s_dev_hdl);
        break;

    case USB_DEVICE_EVENT_POWER_REMOVED:
        USB_DEVICE_Detach(s_dev_hdl);
        s_cdc_configured = false;
        s_cdc_tx_busy    = false;
        break;

    case USB_DEVICE_EVENT_SUSPENDED:
    case USB_DEVICE_EVENT_RESUMED:
    case USB_DEVICE_EVENT_ERROR:
    case USB_DEVICE_EVENT_SOF:
    default:
        break;
    }
}

/* --------------------------------------------------------------------------
 * USB CDC event handler
 * -------------------------------------------------------------------------- */
static void usb_cdc_event_handler(USB_DEVICE_CDC_INDEX index,
                                  USB_DEVICE_CDC_EVENT event,
                                  void *pData,
                                  uintptr_t context)
{
    (void)index;
    (void)pData;
    (void)context;
    switch (event) {
    case USB_DEVICE_CDC_EVENT_WRITE_COMPLETE:
        s_cdc_tx_busy = false;
        break;

    case USB_DEVICE_CDC_EVENT_READ_COMPLETE:
    case USB_DEVICE_CDC_EVENT_SET_LINE_CODING:
    case USB_DEVICE_CDC_EVENT_GET_LINE_CODING:
    case USB_DEVICE_CDC_EVENT_SET_CONTROL_LINE_STATE:
    case USB_DEVICE_CDC_EVENT_SEND_BREAK:
    case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_RECEIVED:
    case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_SENT:
    default:
        break;
    }
}

/* --------------------------------------------------------------------------
 * USB1_Initialize
 * -------------------------------------------------------------------------- */
void USB1_Initialize(void)
{
    static const DRV_USBFS_INIT drv_init = {
        .endpointTable       = s_endpointTable,
        .interruptSource     = INT_SOURCE_USB_1,
        .operationMode       = DRV_USBFS_OPMODE_DEVICE,
        .operationSpeed      = USB_SPEED_FULL,
        .usbID               = (USB_MODULE_ID)_USB_BASE_ADDRESS,
    };

    s_drv_obj = DRV_USBFS_Initialize(DRV_USBFS_INDEX_0,
                    (SYS_MODULE_INIT *)(uintptr_t)&drv_init);
    sysObj.drvUSBFSObject0 = s_drv_obj;

    s_dev_obj = USB_DEVICE_Initialize(USB_DEVICE_INDEX_0,
                    (SYS_MODULE_INIT *)(uintptr_t)&usbDevInitData0);
    sysObj.usbDevObject0 = s_dev_obj;

    /* Set USB1 interrupt priority = 1 in IPC8[20:18] (source 34, shift=16, prio at +2) */
    IPC8CLR = _IPC8_USB1IP_MASK | _IPC8_USB1IS_MASK;
    IPC8SET = (1u << _IPC8_USB1IP_POSITION);

    /* Enable USB1 interrupt in EVIC: source 34 = IFS1 bit 2 */
    IFS1CLR = _IFS1_USB1IF_MASK;
    IEC1SET = _IEC1_USB1IE_MASK;

    /* NOTE: USB_DEVICE_Open() / USB_DEVICE_Attach() MUST be called from
     * vUsbDeviceTask() AFTER USB_DEVICE_Tasks() has advanced the internal
     * state machine to SYS_STATUS_READY (USB_DEVICE_TASK_STATE_OPENING_USBCD
     * → USB_DEVICE_TASK_STATE_RUNNING).  Calling Open() here (before the
     * scheduler starts) always returns USB_DEVICE_HANDLE_INVALID because the
     * device layer has not yet opened the underlying USBFS driver. */
}

/* --------------------------------------------------------------------------
 * USB1_InterruptHandler — called from vUSB1InterruptWrapper (ISR context)
 * -------------------------------------------------------------------------- */
void USB1_InterruptHandler(void)
{
    DRV_USBFS_Tasks_ISR(s_drv_obj);
}

/* --------------------------------------------------------------------------
 * USB_SendFrame
 * -------------------------------------------------------------------------- */
bool USB_SendFrame(const uint8_t *data, size_t len)
{
    USB_DEVICE_CDC_TRANSFER_HANDLE handle;
    USB_DEVICE_CDC_RESULT result;

    if (!s_cdc_configured || s_cdc_tx_busy || len == 0U) {
        return false;
    }
    s_cdc_tx_busy = true;
    result = USB_DEVICE_CDC_Write(
        USB_DEVICE_CDC_INDEX_0,
        &handle,
        (void *)(uintptr_t)data,
        len,
        USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
    if (result != USB_DEVICE_CDC_RESULT_OK) {
        s_cdc_tx_busy = false;
        return false;
    }
    return true;
}

/* --------------------------------------------------------------------------
 * vUsbDeviceTask — must run at least as fast as USB SOF (every 1 ms);
 * 10 ms is sufficient for CDC throughput in this test.
 *
 * Phase 1: Poll USB_DEVICE_Tasks() until the internal state machine opens
 *          the underlying USBFS driver and sets the device layer to READY
 *          (USB_DEVICE_TASK_STATE_OPENING_USBCD → TASK_STATE_RUNNING).
 *          Only then does USB_DEVICE_Open() return a valid handle.
 * Phase 2: Register event handler and attach (writes 0xDF to UxIE).
 * Phase 3: Normal 10 ms task loop.
 * -------------------------------------------------------------------------- */
void vUsbDeviceTask(void *pvParam)
{
    (void)pvParam;

    /* Phase 1: wait for device layer to become READY */
    for (;;) {
        USB_DEVICE_Tasks(s_dev_obj);
        s_dev_hdl = USB_DEVICE_Open(USB_DEVICE_INDEX_0,
                                    DRV_IO_INTENT_READWRITE);
        if (s_dev_hdl != USB_DEVICE_HANDLE_INVALID) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Phase 2: register handler and attach */
    USB_DEVICE_EventHandlerSet(s_dev_hdl, usb_device_event_handler, 0);
    USB_DEVICE_Attach(s_dev_hdl);

    /* Phase 3: normal task loop */
    for (;;) {
        USB_DEVICE_Tasks(s_dev_obj);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
