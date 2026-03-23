/*
 * system/debug/sys_debug.h — empty stub for QEMU FreeRTOS test build.
 * All SYS_DEBUG_* macros are already defined as no-ops in configuration.h.
 */
#ifndef SYS_DEBUG_H
#define SYS_DEBUG_H

#ifndef SYS_DEBUG_PRINT
#define SYS_DEBUG_PRINT(level, format, ...)
#endif
#ifndef SYS_DEBUG_MESSAGE
#define SYS_DEBUG_MESSAGE(a, b, ...)
#endif
#ifndef SYS_DEBUG
#define SYS_DEBUG(a, b)
#endif

typedef enum {
    SYS_ERROR_FATAL   = 0,
    SYS_ERROR_ERROR   = 1,
    SYS_ERROR_WARNING = 2,
    SYS_ERROR_INFO    = 3,
    SYS_ERROR_DEBUG   = 4,
} SYS_ERROR_LEVEL;

#endif /* SYS_DEBUG_H */
