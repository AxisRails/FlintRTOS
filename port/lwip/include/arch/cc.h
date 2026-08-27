/*
 * FlintRTOS - lwIP port: compiler/architecture abstraction (arch/cc.h).
 * Target: AArch64 (Cortex-A72, little-endian), clang/LLVM, freestanding.
 */
#ifndef FLINT_LWIP_CC_H
#define FLINT_LWIP_CC_H

#include <stdint.h>
#include <stddef.h>

/* lwIP uses stdint-based types (LWIP_NO_STDINT_H stays 0). */

/* Byte order (AArch64 runs little-endian in this port). */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN    4321
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

/* Struct packing for on-the-wire protocol headers (GCC/clang). */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Diagnostics routed to the FlintRTOS UART console. */
void uart_printf(const char *fmt, ...);
#define LWIP_PLATFORM_DIAG(x) do { uart_printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) \
    do { uart_printf("lwIP assert: \"%s\" (%s:%d)\n", (x), __FILE__, __LINE__); \
         for (;;) { } } while (0)

typedef unsigned int sys_prot_t;  /* saved DAIF state (lightweight protection) */

/* PRNG seam (implemented in sys_arch.c). */
unsigned int lwip_port_rand(void);
#define LWIP_RAND() ((u32_t)lwip_port_rand())

#endif /* FLINT_LWIP_CC_H */
