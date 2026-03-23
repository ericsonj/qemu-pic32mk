/*
 * device.h — QEMU-side shim for Microchip Harmony's "device.h"
 *
 * The Harmony-generated plib source files include <device.h> which on XC32
 * pulls in the processor-specific header (<xc.h>) plus sys/attribs.h.
 * Here we redirect to our GCC compatibility xc.h and provide the missing
 * toolchain_specifics defines.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include "xc.h"

/* toolchain_specifics.h stub — only the macros the plib actually uses */
#ifndef KEEP
#define KEEP  __attribute__((used))
#endif

#ifndef COHERENT
#define COHERENT
#endif

#ifndef CACHE_ALIGN
#define CACHE_ALIGN  __attribute__((aligned(16)))
#endif

#endif /* DEVICE_H */
