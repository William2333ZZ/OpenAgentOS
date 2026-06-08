#ifndef TLS_CLIENT_H
#define TLS_CLIENT_H

#include <stdint.h>

int tls_client_connect(const char *host, uint16_t port, int timeout_ms);
int tls_client_connect_ip(const char *sni_host, uint32_t ip, uint16_t port, int timeout_ms);
int dns_doh_resolve_a(const char *host, uint32_t *out_ip, int timeout_ms);
int tls_client_write(const void *data, int len);
int tls_client_read(void *buf, int len, int timeout_ms);
void tls_client_close(void);

#endif
