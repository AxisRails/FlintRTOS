/* FlintRTOS lwIP port - freestanding <stdlib.h> shim. */
#ifndef FLINT_LWIP_STDLIB_H
#define FLINT_LWIP_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void  free(void *);
void *calloc(size_t, size_t);
void  abort(void);
int   atoi(const char *);
long  strtol(const char *, char **, int);
int   rand(void);
void  srand(unsigned int);
#endif
