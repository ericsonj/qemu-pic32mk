/*
 * system/reset/sys_reset.h — empty stub for QEMU FreeRTOS test build.
 */
#ifndef SYS_RESET_H
#define SYS_RESET_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RESET_CAUSE_NONE = 0,
    RESET_CAUSE_POR  = 1,
    RESET_CAUSE_MCLR = 2,
} RESET_CAUSE;

static inline RESET_CAUSE SYS_RESET_ReasonGet(void) { return RESET_CAUSE_POR; }
static inline void SYS_RESET_ReasonClear(RESET_CAUSE cause) { (void)cause; }

#endif /* SYS_RESET_H */
