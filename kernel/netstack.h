#ifndef NETSTACK_H
#define NETSTACK_H

#include <stdint.h>

#define NET_MTU          1500
#define NET_FRAME_MAX    (NET_MTU + 64)
#define NET_TCP_PORT_MAX 65535

/* QEMU user networking defaults (slirp) */
#define NET_IP_HOST      0x0a00020fUL /* 10.0.2.15 */
#define NET_GW_HOST      0x0a000202UL /* 10.0.2.2  */
#define NET_DNS_HOST     0x08080808UL /* 8.8.8.8 (avoid slirp 10.0.2.3 host hijack) */
#define NET_MASK_HOST    0xffffff00UL

void netstack_init(void);
int netstack_ready(void);
void netstack_poll(void);

int net_dns_resolve(const char *host, uint32_t *out_ip, int timeout_ms);
int net_dns_resolve_slirp(const char *host, uint32_t *out_ip, int timeout_ms);
int net_tcp_connect_ip(uint32_t ip, uint16_t port, int timeout_ms);
int net_tcp_connect_host(const char *host, uint16_t port, int timeout_ms);
int net_tcp_write(const void *data, int len);
int net_tcp_read(void *buf, int buflen, int timeout_ms);
void net_tcp_close(void);

int net_http_get(const char *url, char *body, int body_len);
int net_http_post(const char *host, uint16_t port, const char *path, const char *headers,
                  const char *body, char *resp, int resp_len, int timeout_ms);
int net_https_post_port(const char *host, uint16_t port, const char *path, const char *headers,
                        const char *body, char *resp, int resp_len, int timeout_ms);
int net_https_post(const char *host, const char *path, const char *headers,
                   const char *body, char *resp, int resp_len, int timeout_ms);
int net_https_post_ip(const char *host, uint32_t ip, uint16_t port, const char *path,
                      const char *headers, const char *body, char *resp, int resp_len,
                      int timeout_ms);

#endif
