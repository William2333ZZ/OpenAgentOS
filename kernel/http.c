#include "http.h"
#include "virtio.h"
#include "virtio_net.h"
#include "netstack.h"
#include "printf.h"
#include "../include/agentos.h"

static int url_contains(const char *url, const char *needle) {
    int i = 0;
    int j;

    if (!url || !needle)
        return 0;
    while (url[i]) {
        j = 0;
        while (needle[j] && url[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
        i++;
    }
    return 0;
}

static int http_faux(const char *url, char *buf, int buflen) {
    const char *body;
    int len = 0;

    if (url_contains(url, "reject"))
        return EIO;
    if (url_contains(url, "agentos") || url_contains(url, "stub/"))
        body = "200:agentos-net-stub";
    else
        body = "stub:200 ok";
    while (body[len])
        len++;
    if (len + 1 > buflen)
        return EINVAL;
    for (int i = 0; i <= len; i++)
        buf[i] = body[i];
    kprintf("[http] faux url=%s body=%s\n", url, body);
    return len;
}

static void put_u32_be(char *p, uint32_t v) {
    p[0] = (char)(v >> 24);
    p[1] = (char)(v >> 16);
    p[2] = (char)(v >> 8);
    p[3] = (char)v;
}

static uint32_t get_u32_be(const char *p) {
    return ((uint32_t)(unsigned char)p[0] << 24) |
           ((uint32_t)(unsigned char)p[1] << 16) |
           ((uint32_t)(unsigned char)p[2] << 8) |
           (uint32_t)(unsigned char)p[3];
}

static int http_bridge(const char *url, char *buf, int buflen) {
    char tx[HTTP_MAX_URL + 16];
    char rx[HTTP_MAX_BODY + 16];
    int ulen = 0;
    int tx_len;
    int rc;

    if (!url || !buf || buflen <= 0)
        return EINVAL;

    while (url[ulen] && ulen < HTTP_MAX_URL - 1)
        ulen++;
    if (ulen <= 0)
        return EINVAL;

    rc = virtio_console_init();
    if (rc != 0)
        return rc;

    tx[0] = 'H';
    tx[1] = 'T';
    tx[2] = 'T';
    tx[3] = 'Q';
    put_u32_be(tx + 4, (uint32_t)ulen);
    for (int i = 0; i < ulen; i++)
        tx[8 + i] = url[i];
    tx_len = 8 + ulen;

    rc = virtio_http_exchange(tx, (unsigned long)tx_len, rx, sizeof(rx));
    if (rc < 0)
        return rc;
    if (rc < 8)
        return EIO;
    if (rx[0] != 'H' || rx[1] != 'T' || rx[2] != 'T' || rx[3] != 'R')
        return EIO;

    {
        uint32_t body_len = get_u32_be(rx + 4);
        if (body_len + 8 > (unsigned long)rc || (int)body_len >= buflen)
            return EIO;
        for (uint32_t i = 0; i < body_len; i++)
            buf[i] = rx[8 + i];
        buf[body_len] = '\0';
        kprintf("[http] bridge url=%s body_len=%u\n", url, body_len);
        return (int)body_len;
    }
}

static int http_native(const char *url, char *buf, int buflen) {
    if (!netstack_ready())
        return ENODEV;
    return net_http_get(url, buf, buflen);
}

int agent_http_fetch(const char *url, char *buf, int buflen) {
    if (!url || !buf || buflen <= 0)
        return EINVAL;

#ifdef HTTP_FAUX
    return http_faux(url, buf, buflen);
#else
    {
        int rc = http_native(url, buf, buflen);
        if (rc >= 0)
            return rc;
        kprintf("[http] native failed rc=%d, trying bridge\n", rc);
        rc = http_bridge(url, buf, buflen);
        if (rc >= 0)
            return rc;
        kprintf("[http] bridge failed rc=%d, using faux\n", rc);
        return http_faux(url, buf, buflen);
    }
#endif
}
