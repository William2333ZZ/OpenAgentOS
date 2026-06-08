#include "netstack.h"
#include "../include/agentos.h"

void netstack_init(void) {}

int netstack_ready(void) {
    return 0;
}

void netstack_poll(void) {}

int net_dns_resolve(const char *host, uint32_t *out_ip, int timeout_ms) {
    (void)host;
    (void)out_ip;
    (void)timeout_ms;
    return ENODEV;
}

int net_tcp_connect_ip(uint32_t ip, uint16_t port, int timeout_ms) {
    (void)ip;
    (void)port;
    (void)timeout_ms;
    return ENODEV;
}

int net_tcp_connect_host(const char *host, uint16_t port, int timeout_ms) {
    (void)host;
    (void)port;
    (void)timeout_ms;
    return ENODEV;
}

int net_tcp_write(const void *data, int len) {
    (void)data;
    (void)len;
    return ENODEV;
}

int net_tcp_read(void *buf, int buflen, int timeout_ms) {
    (void)buf;
    (void)buflen;
    (void)timeout_ms;
    return ENODEV;
}

void net_tcp_close(void) {}

int net_http_get(const char *url, char *body, int body_len) {
    (void)url;
    (void)body;
    (void)body_len;
    return ENODEV;
}

int net_http_post(const char *host, uint16_t port, const char *path, const char *headers,
                  const char *body, char *resp, int resp_len, int timeout_ms) {
    (void)host;
    (void)port;
    (void)path;
    (void)headers;
    (void)body;
    (void)resp;
    (void)resp_len;
    (void)timeout_ms;
    return ENODEV;
}

int net_https_post(const char *host, const char *path, const char *headers, const char *body,
                   char *resp, int resp_len, int timeout_ms) {
    (void)host;
    (void)path;
    (void)headers;
    (void)body;
    (void)resp;
    (void)resp_len;
    (void)timeout_ms;
    return ENODEV;
}

int net_https_post_ip(const char *host, uint32_t ip, const char *path, const char *headers,
                      const char *body, char *resp, int resp_len, int timeout_ms) {
    (void)host;
    (void)ip;
    (void)path;
    (void)headers;
    (void)body;
    (void)resp;
    (void)resp_len;
    (void)timeout_ms;
    return ENODEV;
}
