/* FlintRTOS lwIP port - freestanding <stdio.h> shim (formatting subset). */
#ifndef FLINT_LWIP_STDIO_H
#define FLINT_LWIP_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int sprintf(char *, const char *, ...);
int printf(const char *, ...);
#endif
