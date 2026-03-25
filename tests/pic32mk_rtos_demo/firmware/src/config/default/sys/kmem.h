/*
 * Minimal sys/kmem.h stub for mipsel-linux-gnu-gcc cross-compilation.
 * The real file is part of XC32 toolchain.
 */
#ifndef SYS_KMEM_H
#define SYS_KMEM_H
#include <stdint.h>
#define KVA_TO_PA(v)    ((uint32_t)(uintptr_t)(v) & 0x1FFFFFFFu)
#define PA_TO_KVA0(pa)  ((void *)((uintptr_t)(pa) | 0x80000000u))
#define PA_TO_KVA1(pa)  ((void *)((uintptr_t)(pa) | 0xA0000000u))
#define KVA0_TO_KVA1(v) ((void *)(((uintptr_t)(v) & 0x1FFFFFFFu) | 0xA0000000u))
#endif /* SYS_KMEM_H */
