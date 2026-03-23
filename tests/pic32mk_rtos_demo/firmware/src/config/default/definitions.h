/*
 * definitions.h — Trimmed Harmony 3 definitions for QEMU FreeRTOS test build.
 *
 * The full firmware definitions.h includes hundreds of peripheral headers that
 * don't exist in our minimal build. This version only includes what the USB
 * driver stack actually needs.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "configuration.h"
#include "device.h"
#include "system/system_common.h"
#include "system/system_module.h"
#include "system/int/sys_int.h"
#include "osal/osal.h"
#include "usb/usb_chapter_9.h"
#include "usb/usb_common.h"
#include "usb/usb_device.h"
#include "usb/usb_device_cdc.h"
#include "usb/usb_cdc.h"
#include "driver/usb/drv_usb.h"
#include "driver/usb/usbfs/drv_usbfs.h"

/* SYSTEM_OBJECTS — mirrors firmware's definitions.h §SYSTEM_OBJECTS block.
 * DRV_USBFS_USB1_Handler() in the copied drv_usbfs.c references sysObj. */
typedef struct {
    SYS_MODULE_OBJ  usbDevObject0;
    SYS_MODULE_OBJ  drvUSBFSObject0;
} SYSTEM_OBJECTS;

extern SYSTEM_OBJECTS sysObj;

#endif /* DEFINITIONS_H */
