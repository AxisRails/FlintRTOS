/* Host (Linux) lwIP arch for tests/mqtt_lwip_test.c. */
#ifndef FLINT_TEST_LWIP_CC_H
#define FLINT_TEST_LWIP_CC_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#define LWIP_PLATFORM_DIAG(x)   do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("lwIP assert: %s (%s:%d)\n", (x), __FILE__, __LINE__); abort(); } while (0)
#define LWIP_RAND() ((u32_t)rand())
#endif
