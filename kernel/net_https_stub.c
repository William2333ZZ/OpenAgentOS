#include "netstack.h"
#include "../include/agentos.h"

int net_https_post_port(const char *host, uint16_t port, const char *path,
                        const char *headers, const char *body, char *resp,
                        int resp_len, int timeout_ms) {
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

int net_https_post(const char *host, const char *path, const char *headers,
                   const char *body, char *resp, int resp_len, int timeout_ms) {
    return net_https_post_port(host, 443, path, headers, body, resp, resp_len, timeout_ms);
}

int net_https_post_ip(const char *host, uint32_t ip, uint16_t port,
                      const char *path, const char *headers, const char *body,
                      char *resp, int resp_len, int timeout_ms) {
    (void)host;
    (void)ip;
    (void)port;
    (void)path;
    (void)headers;
    (void)body;
    (void)resp;
    (void)resp_len;
    (void)timeout_ms;
    return ENODEV;
}
