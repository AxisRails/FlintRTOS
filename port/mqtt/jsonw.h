/*
 * FlintRTOS - minimal bounded JSON writer (no libc, no floating point).
 * Writes are truncated safely at the buffer end; jw_len() reports what fits,
 * jw_ok() whether everything did.
 */
#ifndef FLINT_JSONW_H
#define FLINT_JSONW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
    char  *buf;
    size_t cap;
    size_t len;
    bool   overflow;
    bool   need_comma;
} JsonW;

void   jw_init(JsonW *w, char *buf, size_t cap);
void   jw_obj_open(JsonW *w);              /* {   (as a value) */
void   jw_obj_close(JsonW *w);             /* }               */
void   jw_arr_open(JsonW *w, const char *key);   /* "key":[       */
void   jw_arr_close(JsonW *w);             /* ]               */
void   jw_str(JsonW *w, const char *key, const char *val);
void   jw_i64(JsonW *w, const char *key, int64_t val);
void   jw_u32(JsonW *w, const char *key, uint32_t val);
void   jw_bool(JsonW *w, const char *key, bool val);
size_t jw_len(const JsonW *w);
bool   jw_ok(const JsonW *w);

/* Plain helpers. */
size_t fm_strlen(const char *s);
void   fm_strlcpy(char *dst, const char *src, size_t cap);
void   fm_strlcat(char *dst, const char *src, size_t cap);
char  *fm_u64_to_str(uint64_t v, char *out);       /* returns out */
char  *fm_hex64(uint64_t v, char *out, unsigned digits);

#endif /* FLINT_JSONW_H */
