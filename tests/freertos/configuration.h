/*
 * configuration.h — Harmony 3 build-time configuration for QEMU FreeRTOS test.
 *
 * Mirrors the relevant macros from the firmware's configuration.h, reduced to
 * only what the USB CDC stack requires. Must be included before any Harmony
 * source files.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdint.h>
#include <stdbool.h>

/* ----- USB Device Layer ----- */
#define USB_DEVICE_INSTANCES_NUMBER              1U
#define USB_DEVICE_EP0_BUFFER_SIZE               64U
/* Device layer will NOT auto-init the USB driver (we call DRV_USBFS_Initialize explicitly) */
#define USB_DEVICE_DRIVER_INITIALIZE_EXPLICIT
#define USB_DEVICE_SOF_EVENT_ENABLE

/* ----- USB FS OTG Driver ----- */
#define DRV_USBFS_INSTANCES_NUMBER               1U
#define DRV_USBFS_INTERRUPT_MODE                 true
#define DRV_USBFS_DEVICE_SUPPORT                 true
#define DRV_USBFS_HOST_SUPPORT                   false
#define DRV_USBFS_ENDPOINTS_NUMBER               3U   /* EP0 control + EP1 bulk + EP2 interrupt */

/* Buffer alignment for USB DMA — 16-byte cache-line aligned */
#define USB_ALIGN                                __attribute__((aligned(16)))
#ifndef CACHE_ALIGN
#define CACHE_ALIGN                              __attribute__((aligned(16)))
#endif

/* ----- USB CDC Function Driver ----- */
#define USB_DEVICE_CDC_INSTANCES_NUMBER          1U
#define USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED      3U   /* shared read+write+notify queue depth */

/* ----- System debug (disabled in test build) ----- */
#define SYS_DEBUG_PRINT(level, format, ...)      /* no-op */
#define SYS_DEBUG_MESSAGE(a, b, ...)             /* no-op */
#define SYS_DEBUG(a, b)                          /* no-op */

/* ----- MISRA deviation suppression (no MISRA in test build) ----- */
#define MISRA_SUPPRESS_NULL_PTR_DEREFERENCE(...)

/* ----- XC32 compatibility for Harmony headers compiled with GCC ----- */
/* osal_freertos.h uses __STATIC_INLINE; ARM CMSIS defines it as static __inline */
#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

#endif /* CONFIGURATION_H */
