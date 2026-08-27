/*
 * FlintRTOS - lwIP port: sys_arch types.
 * NO_SYS=1 mode uses only sys_now() + lightweight protection; the OS-mode
 * sem/mbox/thread types are declared for a future sequential/socket build.
 */
#ifndef FLINT_LWIP_SYS_ARCH_H
#define FLINT_LWIP_SYS_ARCH_H

#include <stdint.h>

/* OS-mode primitives (unused while NO_SYS=1; provided for the later port). */
typedef struct { void *q; }  sys_sem_t;
typedef struct { void *q; }  sys_mutex_t;
typedef struct { void *q; }  sys_mbox_t;
typedef void *               sys_thread_t;

#endif /* FLINT_LWIP_SYS_ARCH_H */
