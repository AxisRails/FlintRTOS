/*
 * FlintRTOS - minimal freestanding libc primitives for the bare-metal target.
 * The compiler may emit calls to these for aggregate copies/initialisation even
 * with -ffreestanding, and modules such as queue.c use memcpy. TCB (MISRA C).
 * Not compiled into host unit-test builds (which link the system C library).
 */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0U; i < n; i++)
    {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *dest, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0U; i < n; i++)
    {
        d[i] = (uint8_t)c;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s)
    {
        for (size_t i = 0U; i < n; i++)
        {
            d[i] = s[i];
        }
    }
    else
    {
        for (size_t i = n; i > 0U; i--)
        {
            d[i - 1U] = s[i - 1U];
        }
    }
    return dest;
}

size_t strlen(const char *s)
{
    size_t n = 0U;
    while (s[n] != '\0')
    {
        n++;
    }
    return n;
}
