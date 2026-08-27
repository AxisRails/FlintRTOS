/*
 * FlintRTOS - lwIP port: freestanding libc support used only by the lwIP build.
 * Complements kernel/flint_libc.c (memcpy/memset/memmove/strlen) with the extra
 * string/stdlib/stdio functions lwIP references. Bare-metal, no host libc.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* Heap from the selected MemMang/heap_N.c */
void  *pvPortMalloc(size_t);
void   vPortFree(void *);

/* ---- string.h extras (memcpy/memset/memmove/strlen are in flint_libc.c) --- */
void *memchr(const void *s, int c, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    for (size_t i = 0U; i < n; i++)
    {
        if (p[i] == (uint8_t)c) { return (void *)(p + i); }
    }
    return NULL;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0U; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            return (int)pa[i] - (int)pb[i];
        }
    }
    return 0;
}

int strcmp(const char *a, const char *b)
{
    while ((*a != '\0') && (*a == *b)) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0U; i < n; i++)
    {
        if ((a[i] != b[i]) || (a[i] == '\0'))
        {
            return (int)(uint8_t)a[i] - (int)(uint8_t)b[i];
        }
    }
    return 0;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++) != '\0') { }
    return r;
}

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0U;
    for (; (i < n) && (s[i] != '\0'); i++) { d[i] = s[i]; }
    for (; i < n; i++) { d[i] = '\0'; }
    return d;
}

char *strchr(const char *s, int c)
{
    while (*s != '\0')
    {
        if (*s == (char)c) { return (char *)s; }
        s++;
    }
    return ((char)c == '\0') ? (char *)s : NULL;
}

char *strstr(const char *hay, const char *needle)
{
    if (*needle == '\0') { return (char *)hay; }
    for (; *hay != '\0'; hay++)
    {
        const char *h = hay;
        const char *n = needle;
        while ((*h != '\0') && (*n != '\0') && (*h == *n)) { h++; n++; }
        if (*n == '\0') { return (char *)hay; }
    }
    return NULL;
}

/* ---- stdlib.h ------------------------------------------------------------ */
void *malloc(size_t n)            { return pvPortMalloc(n); }
void  free(void *p)              { vPortFree(p); }
void *calloc(size_t a, size_t b)
{
    size_t total = a * b;
    void  *p = pvPortMalloc(total);
    if (p != NULL)
    {
        uint8_t *d = (uint8_t *)p;
        for (size_t i = 0U; i < total; i++) { d[i] = 0U; }
    }
    return p;
}

void abort(void)
{
    for (;;) { __asm__ volatile("wfe"); }
}

int atoi(const char *s)
{
    int v = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while ((*s >= '0') && (*s <= '9')) { v = (v * 10) + (*s - '0'); s++; }
    return v * sign;
}

long strtol(const char *s, char **end, int base)
{
    long v = 0;
    int  sign = 1;
    if (*s == '-') { sign = -1; s++; }
    if (base == 16)
    {
        if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X'))) { s += 2; }
    }
    for (;;)
    {
        int d;
        char c = *s;
        if ((c >= '0') && (c <= '9'))      { d = c - '0'; }
        else if ((c >= 'a') && (c <= 'f')) { d = (c - 'a') + 10; }
        else if ((c >= 'A') && (c <= 'F')) { d = (c - 'A') + 10; }
        else { break; }
        if (d >= base) { break; }
        v = (v * base) + d;
        s++;
    }
    if (end != NULL) { *end = (char *)s; }
    return v * sign;
}

int   rand(void)              { extern unsigned int lwip_port_rand(void); return (int)(lwip_port_rand() & 0x7FFFFFFFU); }
void  srand(unsigned int seed) { (void)seed; }

/* ---- stdio.h: a minimal vsnprintf covering lwIP's format usage ----------- */
static void emit(char *buf, size_t size, size_t *pos, char c)
{
    if (*pos < size) { buf[*pos] = c; }
    (*pos)++;
}

static void emit_str(char *buf, size_t size, size_t *pos, const char *s)
{
    while (*s != '\0') { emit(buf, size, pos, *s); s++; }
}

static void emit_num(char *buf, size_t size, size_t *pos,
                     unsigned long v, unsigned int base, int is_signed, int neg)
{
    char tmp[24];
    const char *digits = "0123456789abcdef";
    size_t i = 0U;
    (void)is_signed;
    if (neg) { emit(buf, size, pos, '-'); }
    if (v == 0U) { emit(buf, size, pos, '0'); return; }
    while ((v != 0U) && (i < sizeof(tmp))) { tmp[i++] = digits[v % base]; v /= base; }
    while (i > 0U) { i--; emit(buf, size, pos, tmp[i]); }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t pos = 0U;
    for (const char *p = fmt; *p != '\0'; p++)
    {
        if (*p != '%') { emit(buf, size, &pos, *p); continue; }
        p++;
        /* skip width/length modifiers we don't interpret */
        while ((*p == 'l') || (*p == 'h') || (*p == 'z') ||
               ((*p >= '0') && (*p <= '9')) || (*p == '-') || (*p == '.') || (*p == '+') || (*p == ' '))
        { p++; }
        switch (*p)
        {
            case 'd': { long v = va_arg(ap, long); emit_num(buf, size, &pos, (v<0)?(unsigned long)(-v):(unsigned long)v, 10U, 1, (v<0)); break; }
            case 'u': emit_num(buf, size, &pos, va_arg(ap, unsigned long), 10U, 0, 0); break;
            case 'x': emit_num(buf, size, &pos, va_arg(ap, unsigned long), 16U, 0, 0); break;
            case 'p': emit_str(buf, size, &pos, "0x"); emit_num(buf, size, &pos, (unsigned long)(uintptr_t)va_arg(ap, void*), 16U, 0, 0); break;
            case 'c': emit(buf, size, &pos, (char)va_arg(ap, int)); break;
            case 's': emit_str(buf, size, &pos, va_arg(ap, const char*)); break;
            case '%': emit(buf, size, &pos, '%'); break;
            case '\0': p--; break;
            default: emit(buf, size, &pos, '%'); emit(buf, size, &pos, *p); break;
        }
    }
    if (size > 0U) { buf[(pos < size) ? pos : (size - 1U)] = '\0'; }
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, (size_t)0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return n;
}

/* errno storage for the shim. */
int errno = 0;
