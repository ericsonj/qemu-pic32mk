/*
 * sys_int_mapping.h — QEMU/GCC override for Harmony 3 interrupt mapping.
 *
 * The firmware's sys_int_mapping.h maps most SYS_INT_* APIs to EVIC_* inline
 * functions but omits SYS_INT_SourceDisable and SYS_INT_SourceRestore (they
 * are declared as real C functions in sys_int.h that link against plib_evic.c,
 * which is XC32-only).  We map them here instead.
 *
 * SYS_INT_SourceDisable: disables the source in IECn, returns previous state.
 * SYS_INT_SourceRestore: re-enables source only if was_enabled is true.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYS_INT_MAPPING_H
#define SYS_INT_MAPPING_H

#include "peripheral/evic/plib_evic.h"

#define SYS_INT_IsEnabled()                 ((bool)(_CP0_GET_STATUS() & 0x01))
#define SYS_INT_SourceEnable(source)        EVIC_SourceEnable(source)
#define SYS_INT_SourceIsEnabled(source)     EVIC_SourceIsEnabled(source)
#define SYS_INT_SourceStatusGet(source)     EVIC_SourceStatusGet(source)
#define SYS_INT_SourceStatusSet(source)     EVIC_SourceStatusSet(source)
#define SYS_INT_SourceStatusClear(source)   EVIC_SourceStatusClear(source)

/* These two are declared as real function prototypes in sys_int.h but are NOT
 * in the firmware's sys_int_mapping.h.  Map them to inline EVIC calls. */
#define SYS_INT_SourceDisable(source)       EVIC_SourceDisable(source)
#define SYS_INT_SourceRestore(source, was)  \
    do { if (was) { EVIC_SourceEnable(source); } } while (0)

#endif /* SYS_INT_MAPPING_H */
