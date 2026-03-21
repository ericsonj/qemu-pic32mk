/*
 * sys_int.h — QEMU/GCC override for Harmony 3 SYS_INT API.
 *
 * The firmware's sys_int.h declares SYS_INT_SourceDisable and
 * SYS_INT_SourceRestore as real C function prototypes (not macros) and
 * then includes sys_int_mapping.h which maps only some APIs to EVIC_ inlines.
 * Our override adds the missing mappings so everything is inlined.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYS_INT_H
#define SYS_INT_H

#include <stdint.h>
#include <stdbool.h>
#include "peripheral/evic/plib_evic.h"

typedef uint32_t INT_SOURCE;

/* Include the mapping (which provides all SYS_INT_* → EVIC_* macros,
 * including SYS_INT_SourceDisable and SYS_INT_SourceRestore). */
#include "sys_int_mapping.h"

#endif /* SYS_INT_H */
