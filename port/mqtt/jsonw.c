/*
 * FlintRTOS - minimal bounded JSON writer. See jsonw.h.
 */
#include "jsonw.h"

size_t fm_strlen(const char *s)
{
    size_t n = 0U;
    while (s[n] != '\0') { n++; }
    return n;
}

void fm_strlcpy(char *dst, const char *src, size_t cap)
{
    size_t i = 0U;
    if (cap == 0U) { return; }
    while ((src[i] != '\0') && (i < (cap - 1U))) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void fm_strlcat(char *dst, const char *src, size_t cap)
{
    size_t n = fm_strlen(dst);
    if (n < cap) { fm_strlcpy(&dst[n], src, cap - n); }
}

char *fm_u64_to_str(uint64_t v, char *out)
{
    char tmp[21];
    size_t n = 0U, o = 0U;
    do { tmp[n++] = (char)('0' + (char)(v % 10U)); v /= 10U; } while (v != 0U);
    while (n > 0U) { out[o++] = tmp[--n]; }
    out[o] = '\0';
    return out;
}

char *fm_hex64(uint64_t v, char *out, unsigned digits)
{
    static const char hx[] = "0123456789abcdef";
    for (unsigned i = 0U; i < digits; i++)
    {
        out[digits - 1U - i] = hx[v & 0xFU];
        v >>= 4;
    }
    out[digits] = '\0';
    return out;
}

static void put_c(JsonW *w, char c)
{
    if ((w->len + 1U) < w->cap) { w->buf[w->len++] = c; w->buf[w->len] = '\0'; }
    else                        { w->overflow = true; }
}

static void put_s(JsonW *w, const char *s)
{
    while (*s != '\0') { put_c(w, *s); s++; }
}

static void put_escaped(JsonW *w, const char *s)
{
    put_c(w, '"');
    for (; *s != '\0'; s++)
    {
        char c = *s;
        if ((c == '"') || (c == '\\')) { put_c(w, '\\'); put_c(w, c); }
        else if ((unsigned char)c < 0x20U) { put_c(w, ' '); }   /* drop controls */
        else { put_c(w, c); }
    }
    put_c(w, '"');
}

static void sep(JsonW *w)
{
    if (w->need_comma) { put_c(w, ','); }
    w->need_comma = true;
}

static void key(JsonW *w, const char *k)
{
    sep(w);
    if (k != NULL) { put_escaped(w, k); put_c(w, ':'); }
}

void jw_init(JsonW *w, char *buf, size_t cap)
{
    w->buf = buf; w->cap = cap; w->len = 0U;
    w->overflow = false; w->need_comma = false;
    if (cap > 0U) { buf[0] = '\0'; }
}

void jw_obj_open(JsonW *w)  { sep(w); put_c(w, '{'); w->need_comma = false; }
void jw_obj_close(JsonW *w) { put_c(w, '}'); w->need_comma = true; }
void jw_arr_open(JsonW *w, const char *k) { key(w, k); put_c(w, '['); w->need_comma = false; }
void jw_arr_close(JsonW *w) { put_c(w, ']'); w->need_comma = true; }

void jw_str(JsonW *w, const char *k, const char *v) { key(w, k); put_escaped(w, v); }

void jw_i64(JsonW *w, const char *k, int64_t v)
{
    char num[22];
    key(w, k);
    if (v < 0) { put_c(w, '-'); put_s(w, fm_u64_to_str((uint64_t)(-(v + 1)) + 1U, num)); }
    else       { put_s(w, fm_u64_to_str((uint64_t)v, num)); }
}

void jw_u32(JsonW *w, const char *k, uint32_t v)
{
    char num[22];
    key(w, k);
    put_s(w, fm_u64_to_str(v, num));
}

void jw_bool(JsonW *w, const char *k, bool v) { key(w, k); put_s(w, v ? "true" : "false"); }

size_t jw_len(const JsonW *w) { return w->len; }
bool   jw_ok(const JsonW *w)  { return !w->overflow; }
