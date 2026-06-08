#include "mem.h"
#include "printf.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "mbedtls/entropy.h"
#include "mbedtls/platform.h"

static int append_char(char *s, size_t n, size_t *pos, char c) {
    if (*pos + 1 >= n)
        return -1;
    s[(*pos)++] = c;
    s[*pos] = '\0';
    return 0;
}

static int append_str(char *s, size_t n, size_t *pos, const char *txt) {
    while (txt && *txt) {
        if (append_char(s, n, pos, *txt++) != 0)
            return -1;
    }
    return 0;
}

static int append_uint(char *s, size_t n, size_t *pos, unsigned long v, int base, int upper) {
    char tmp[32];
    int ti = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0)
        tmp[ti++] = '0';
    while (v > 0) {
        tmp[ti++] = digits[v % (unsigned)base];
        v /= (unsigned)base;
    }
    while (ti > 0) {
        if (append_char(s, n, pos, tmp[--ti]) != 0)
            return -1;
    }
    return 0;
}

static int mbedtls_vsnprintf(char *s, size_t n, const char *fmt, va_list ap) {
    size_t pos = 0;

    if (!s || n == 0)
        return -1;
    s[0] = '\0';
    if (!fmt)
        return 0;

    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            if (append_char(s, n, &pos, fmt[i]) != 0)
                return (int)pos;
            continue;
        }
        i++;
        if (fmt[i] == 'd') {
            long v = va_arg(ap, long);
            if (v < 0) {
                append_char(s, n, &pos, '-');
                v = -v;
            }
            append_uint(s, n, &pos, (unsigned long)v, 10, 0);
        } else if (fmt[i] == 'u') {
            append_uint(s, n, &pos, va_arg(ap, unsigned long), 10, 0);
        } else if (fmt[i] == 'x') {
            append_uint(s, n, &pos, va_arg(ap, unsigned long), 16, 0);
        } else if (fmt[i] == 'X') {
            append_uint(s, n, &pos, va_arg(ap, unsigned long), 16, 1);
        } else if (fmt[i] == 's') {
            append_str(s, n, &pos, va_arg(ap, const char *));
        } else if (fmt[i] == 'p') {
            append_str(s, n, &pos, "0x");
            append_uint(s, n, &pos, (unsigned long)va_arg(ap, void *), 16, 0);
        } else if (fmt[i] == '%') {
            append_char(s, n, &pos, '%');
        }
    }
    return (int)pos;
}

static uint64_t read_time(void) {
    uint64_t t;
    __asm__ volatile("csrr %0, time" : "=r"(t));
    return t;
}

int mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t),
                                     void (*free_func)(void *)) {
    (void)calloc_func;
    (void)free_func;
    return 0;
}

void *mbedtls_calloc(size_t n, size_t size) {
    unsigned long total = (unsigned long)n * (unsigned long)size;
    void *p = kalloc(total);
    if (!p) {
        kprintf("[tls] kalloc failed n=%u size=%u\n", (unsigned)n, (unsigned)size);
        return 0;
    }
    for (unsigned long i = 0; i < total; i++)
        ((unsigned char *)p)[i] = 0;
    return p;
}

void mbedtls_free(void *ptr) {
    kfree(ptr);
}

int mbedtls_platform_snprintf(char *s, size_t n, const char *fmt, ...) {
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = mbedtls_vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return rc;
}

int mbedtls_platform_printf(const char *fmt, ...) {
    va_list ap;
    char buf[256];
    int rc;

    va_start(ap, fmt);
    rc = mbedtls_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (rc > 0)
        kprintf("%s", buf);
    return rc;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    uint64_t t = read_time();
    size_t i;

    (void)data;
    if (!output || !olen)
        return -1;
    for (i = 0; i < len; i++)
        output[i] = (unsigned char)((t >> (i * 5)) ^ (t >> (i + 3)));
    *olen = len;
    return 0;
}
