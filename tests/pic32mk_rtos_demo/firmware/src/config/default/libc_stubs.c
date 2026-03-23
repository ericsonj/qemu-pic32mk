/*
 * libc_stubs.c — Minimal libc functions for bare-metal FreeRTOS build
 *
 * FreeRTOS sources (tasks.c, queue.c, etc.) call standard libc functions
 * like memset() and memcpy().  Since we build with -nostdlib -ffreestanding,
 * we must provide them ourselves.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stddef.h>

/* FreeRTOS_tasks.c uses memset to zero TCB and stack memory */
void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

/* queue.c uses memcpy to copy item data into/out of queue storage */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}
