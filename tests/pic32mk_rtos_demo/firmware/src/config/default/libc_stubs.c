/*
 * libc_stubs.c — Minimal libc functions for bare-metal FreeRTOS build
 *
 * FreeRTOS sources (tasks.c, queue.c, etc.) call standard libc functions
 * like memset() and memcpy(). Since we build with -nostdlib -ffreestanding,
 * we must provide them ourselves.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

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

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
    {
        p++;
    }

    return (size_t)(p - s);
}

static size_t snprintf_putc(char *dst, size_t size, size_t pos, char c)
{
    if (size > 0U && pos + 1U < size)
    {
        dst[pos] = c;
    }

    return pos + 1U;
}

static size_t snprintf_puts(char *dst, size_t size, size_t pos,
                            const char *s, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        pos = snprintf_putc(dst, size, pos, s[i]);
    }

    return pos;
}

static size_t snprintf_put_unsigned(char *dst, size_t size, size_t pos,
                                    unsigned long value, unsigned int base,
                                    bool uppercase, unsigned int width,
                                    char pad)
{
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t len = 0;

    do
    {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value != 0UL);

    while (len < width)
    {
        buf[len++] = pad;
    }

    while (len > 0U)
    {
        pos = snprintf_putc(dst, size, pos, buf[--len]);
    }

    return pos;
}

static size_t snprintf_put_signed(char *dst, size_t size, size_t pos,
                                  long value, unsigned int width, char pad)
{
    unsigned long magnitude;

    if (value < 0)
    {
        magnitude = (unsigned long)(-(value + 1L)) + 1UL;
        if (pad == '0' && width > 0U)
        {
            pos = snprintf_putc(dst, size, pos, '-');
            width--;
            return snprintf_put_unsigned(dst, size, pos, magnitude, 10U,
                                         false, width, pad);
        }

        magnitude = (unsigned long)(-(value + 1L)) + 1UL;
        if (width > 1U)
        {
            width--;
        }
        pos = snprintf_put_unsigned(dst, size, pos, magnitude, 10U,
                                    false, width, pad);
        return pos;
    }

    return snprintf_put_unsigned(dst, size, pos, (unsigned long)value, 10U,
                                 false, width, pad);
}

int vsnprintf(char *dst, size_t size, const char *format, va_list args)
{
    size_t pos = 0;

    while (*format != '\0')
    {
        if (*format != '%')
        {
            pos = snprintf_putc(dst, size, pos, *format++);
            continue;
        }

        format++;

        if (*format == '\0')
        {
            break;
        }

        {
            char pad = ' ';
            unsigned int width = 0;
            bool long_modifier = false;

            if (*format == '0')
            {
                pad = '0';
                format++;
            }

            while (*format >= '0' && *format <= '9')
            {
                width = (width * 10U) + (unsigned int)(*format - '0');
                format++;
            }

            while (*format == 'l')
            {
                long_modifier = true;
                format++;
            }

            switch (*format)
            {
            case '%':
                pos = snprintf_putc(dst, size, pos, '%');
                break;
            case 'c':
                pos = snprintf_putc(dst, size, pos, (char)va_arg(args, int));
                break;
            case 's':
            {
                const char *s = va_arg(args, const char *);

                if (s == NULL)
                {
                    s = "(null)";
                }

                pos = snprintf_puts(dst, size, pos, s, strlen(s));
                break;
            }
            case 'd':
            case 'i':
                if (long_modifier)
                {
                    pos = snprintf_put_signed(dst, size, pos,
                                              va_arg(args, long), width, pad);
                }
                else
                {
                    pos = snprintf_put_signed(dst, size, pos,
                                              (long)va_arg(args, int), width, pad);
                }
                break;
            case 'u':
                if (long_modifier)
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                va_arg(args, unsigned long),
                                                10U, false, width, pad);
                }
                else
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                (unsigned long)va_arg(args, unsigned int),
                                                10U, false, width, pad);
                }
                break;
            case 'x':
            case 'X':
                if (long_modifier)
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                va_arg(args, unsigned long),
                                                16U, *format == 'X', width, pad);
                }
                else
                {
                    pos = snprintf_put_unsigned(dst, size, pos,
                                                (unsigned long)va_arg(args, unsigned int),
                                                16U, *format == 'X', width, pad);
                }
                break;
            default:
                pos = snprintf_putc(dst, size, pos, '%');
                pos = snprintf_putc(dst, size, pos, *format);
                break;
            }
        }

        format++;
    }

    if (size > 0U)
    {
        dst[(pos < size) ? pos : (size - 1U)] = '\0';
    }

    return (int)pos;
}

int snprintf(char *dst, size_t size, const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = vsnprintf(dst, size, format, args);
    va_end(args);

    return ret;
}

int __vsnprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                    const char *format, va_list args)
{
    (void)flag;
    (void)dstlen;

    return vsnprintf(dst, maxlen, format, args);
}

int __snprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                   const char *format, ...)
{
    int ret;
    va_list args;

    (void)flag;
    (void)dstlen;

    va_start(args, format);
    ret = vsnprintf(dst, maxlen, format, args);
    va_end(args);

    return ret;
}
