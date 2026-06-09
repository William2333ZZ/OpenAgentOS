#include "tls_client.h"
#include "netstack.h"
#include "printf.h"
#include "../include/agentos.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/ecp.h"

#define DOH_IP   0x08080808UL /* 8.8.8.8 */
#define DOH_HOST "dns.google"

static char doh_req[384];
static char doh_resp[2048];

static mbedtls_ssl_context ssl;
static mbedtls_ssl_config conf;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static int tls_active;

static int net_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    (void)ctx;
    return net_tcp_write(buf, (int)len);
}

static int net_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    int n;

    (void)ctx;
    n = net_tcp_read(buf, (int)len, 500);
    if (n <= 0)
        return MBEDTLS_ERR_SSL_WANT_READ;
    return n;
}

static int parse_ipv4_str(const char *s, uint32_t *out) {
    uint32_t ip = 0;
    int oct = 0;
    int val = 0;
    int digits = 0;

    if (!s || !out)
        return 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            digits++;
            if (val > 255)
                return 0;
        } else if (*s == '.') {
            if (digits == 0 || oct >= 3)
                return 0;
            ip = (ip << 8) | (uint32_t)val;
            val = 0;
            digits = 0;
            oct++;
        } else {
            return 0;
        }
        s++;
    }
    if (digits == 0 || oct != 3)
        return 0;
    ip = (ip << 8) | (uint32_t)val;
    *out = ip;
    return 1;
}

static int match_at(const char *p, const char *lit) {
    int i = 0;
    while (lit[i]) {
        if (p[i] != lit[i])
            return 0;
        i++;
    }
    return 1;
}

static int parse_doh_a(const char *json, uint32_t *out_ip) {
    const char *p = json;
    uint32_t last = 0;
    int found = 0;

    while (p[0]) {
        if (match_at(p, "\"type\":1") || match_at(p, "\"type\": 1")) {
            const char *q = p;
            for (int i = 0; i < 120 && q[i]; i++) {
                if (match_at(q + i, "\"data\":\"")) {
                    char ipstr[16];
                    int j = 0;
                    const char *d = q + i + 8;
                    while (d[j] && d[j] != '"' && j < 15) {
                        ipstr[j] = d[j];
                        j++;
                    }
                    ipstr[j] = '\0';
                    if (parse_ipv4_str(ipstr, &last))
                        found = 1;
                    break;
                }
            }
        }
        p++;
    }
    if (!found)
        return EIO;
    *out_ip = last;
    return 0;
}

static int tls_setup_common(const char *sni_host) {
    int rc;
    char pers[] = "agentos-tls";
    static const int ciphersuites[] = {
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        0,
    };
    static const mbedtls_ecp_group_id curves[] = {
        MBEDTLS_ECP_DP_SECP256R1,
        MBEDTLS_ECP_DP_NONE,
    };

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    rc = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char *)pers, sizeof(pers) - 1);
    if (rc != 0)
        return rc;

    rc = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0)
        return rc;

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
    mbedtls_ssl_conf_curves(&conf, curves);

    rc = mbedtls_ssl_setup(&ssl, &conf);
    if (rc != 0)
        return rc;

    rc = mbedtls_ssl_set_hostname(&ssl, sni_host);
    if (rc != 0)
        return rc;

    mbedtls_ssl_set_bio(&ssl, 0, net_bio_send, net_bio_recv, 0);
    return 0;
}

static void tls_cleanup_fail(void) {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    net_tcp_close();
}

int tls_client_connect_ip(const char *sni_host, uint32_t ip, uint16_t port, int timeout_ms) {
    int rc;

    if (!sni_host || port == 0)
        return EINVAL;
    if (tls_active)
        tls_client_close();

    if (net_tcp_connect_ip(ip, port, timeout_ms) != 0) {
        kprintf("[tls] tcp connect ip failed sni=%s port=%u\n", sni_host, (unsigned)port);
        return EIO;
    }

    rc = tls_setup_common(sni_host);
    if (rc != 0)
        goto fail;

    while ((rc = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
            goto fail;
        netstack_poll();
    }

    tls_active = 1;
    kprintf("[tls] connected sni=%s ip=%u.%u.%u.%u port=%u\n", sni_host, (ip >> 24) & 0xff,
            (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, (unsigned)port);
    return 0;

fail:
    {
        char errbuf[80];
        mbedtls_strerror(rc, errbuf, sizeof(errbuf));
        kprintf("[tls] handshake failed rc=%d %s\n", rc, errbuf);
    }
    tls_cleanup_fail();
    return EIO;
}

int tls_client_connect(const char *host, uint16_t port, int timeout_ms) {
    int rc = 0;

    if (!host || port == 0)
        return EINVAL;
    if (tls_active)
        tls_client_close();

    if (net_tcp_connect_host(host, port, timeout_ms) != 0) {
        kprintf("[tls] tcp connect failed host=%s port=%u\n", host, (unsigned)port);
        return EIO;
    }

    rc = tls_setup_common(host);
    if (rc != 0) {
        kprintf("[tls] setup failed rc=%d\n", rc);
        goto fail;
    }

    while ((rc = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
            goto fail;
        netstack_poll();
    }

    tls_active = 1;
    kprintf("[tls] connected host=%s port=%u\n", host, (unsigned)port);
    return 0;

fail:
    kprintf("[tls] handshake failed rc=%d\n", rc);
    tls_cleanup_fail();
    return EIO;
}

int dns_doh_resolve_a(const char *host, uint32_t *out_ip, int timeout_ms) {
    int pos = 0;
    int rlen;
    int i;
    const char *body;
    int rc;

    if (!host || !out_ip || !host[0])
        return EINVAL;

    if (tls_client_connect_ip(DOH_HOST, DOH_IP, 443, timeout_ms) != 0)
        return EIO;

    {
        const char *pfx = "GET /resolve?name=";
        for (i = 0; pfx[i] && pos < (int)sizeof(doh_req) - 96; i++)
            doh_req[pos++] = pfx[i];
    }
    for (i = 0; host[i] && pos < (int)sizeof(doh_req) - 64; i++)
        doh_req[pos++] = host[i];
    {
        const char *sfx = "&type=A HTTP/1.1\r\nHost: dns.google\r\nConnection: close\r\n\r\n";
        for (i = 0; sfx[i] && pos < (int)sizeof(doh_req) - 2; i++)
            doh_req[pos++] = sfx[i];
    }
    doh_req[pos] = '\0';

    if (tls_client_write(doh_req, pos) < 0) {
        tls_client_close();
        return EIO;
    }

    rlen = tls_client_read(doh_resp, (int)sizeof(doh_resp) - 1, timeout_ms);
    tls_client_close();
    if (rlen <= 0)
        return rlen < 0 ? rlen : EIO;
    doh_resp[rlen] = '\0';

    body = doh_resp;
    for (i = 0; i + 3 < rlen; i++) {
        if (doh_resp[i] == '\r' && doh_resp[i + 1] == '\n' && doh_resp[i + 2] == '\r' &&
            doh_resp[i + 3] == '\n') {
            body = doh_resp + i + 4;
            break;
        }
    }

    rc = parse_doh_a(body, out_ip);
    if (rc != 0)
        kprintf("[net] doh parse failed host=%s\n", host);
    return rc;
}

int tls_client_write(const void *data, int len) {
    int rc;
    int sent = 0;

    if (!tls_active || !data || len <= 0)
        return EINVAL;

    while (sent < len) {
        rc = mbedtls_ssl_write(&ssl, (const unsigned char *)data + sent, (size_t)(len - sent));
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            netstack_poll();
            continue;
        }
        if (rc <= 0)
            return EIO;
        sent += rc;
    }
    return sent;
}

int tls_client_read(void *buf, int len, int timeout_ms) {
    int rc;
    int total = 0;
    int idle = 0;

    if (!tls_active || !buf || len <= 0)
        return EINVAL;

    while (total < len && idle < timeout_ms * 20) {
        rc = mbedtls_ssl_read(&ssl, (unsigned char *)buf + total, (size_t)(len - total));
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            netstack_poll();
            idle++;
            continue;
        }
        if (rc == 0)
            break;
        if (rc < 0)
            return total > 0 ? total : EIO;
        total += rc;
        idle = 0;
    }
    return total > 0 ? total : ETIMEDOUT;
}

void tls_client_close(void) {
    if (!tls_active) {
        net_tcp_close();
        return;
    }
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    net_tcp_close();
    tls_active = 0;
}

static char https_req[4096];

static int https_post_send(const char *host, const char *path, const char *headers,
                           const char *body, char *resp, int resp_len, int timeout_ms) {
    int pos = 0;
    int rlen;
    int i;

    {
        const char *pfx = "POST ";
        for (i = 0; pfx[i] && pos < (int)sizeof(https_req) - 256; i++)
            https_req[pos++] = pfx[i];
    }
    for (i = 0; path[i] && pos < (int)sizeof(https_req) - 256; i++)
        https_req[pos++] = path[i];
    {
        const char *mid = " HTTP/1.1\r\nHost: ";
        for (i = 0; mid[i] && pos < (int)sizeof(https_req) - 256; i++)
            https_req[pos++] = mid[i];
    }
    for (i = 0; host[i] && pos < (int)sizeof(https_req) - 256; i++)
        https_req[pos++] = host[i];
    if (headers) {
        https_req[pos++] = '\r';
        https_req[pos++] = '\n';
        for (i = 0; headers[i] && pos < (int)sizeof(https_req) - 128; i++)
            https_req[pos++] = headers[i];
        https_req[pos++] = '\r';
        https_req[pos++] = '\n';
    }
    if (body) {
        char clen[16];
        int ci = 0;
        int bl = 0;
        const char *hdr = "Content-Type: application/json\r\nContent-Length: ";
        for (i = 0; hdr[i] && pos < (int)sizeof(https_req) - 64; i++)
            https_req[pos++] = hdr[i];
        while (body[bl])
            bl++;
        {
            int n = bl;
            char tmp[12];
            int ti = 0;
            if (n == 0)
                clen[ci++] = '0';
            while (n > 0) {
                tmp[ti++] = '0' + (n % 10);
                n /= 10;
            }
            while (ti > 0)
                clen[ci++] = tmp[--ti];
        }
        clen[ci] = '\0';
        for (i = 0; clen[i] && pos < (int)sizeof(https_req) - 32; i++)
            https_req[pos++] = clen[i];
        https_req[pos++] = '\r';
        https_req[pos++] = '\n';
        https_req[pos++] = '\r';
        https_req[pos++] = '\n';
        for (i = 0; body[i] && pos < (int)sizeof(https_req) - 2; i++)
            https_req[pos++] = body[i];
    } else {
        const char *sfx = "\r\nConnection: close\r\n\r\n";
        for (i = 0; sfx[i] && pos < (int)sizeof(https_req) - 2; i++)
            https_req[pos++] = sfx[i];
    }
    https_req[pos] = '\0';

    if (tls_client_write(https_req, pos) < 0) {
        tls_client_close();
        return EIO;
    }

    rlen = tls_client_read(resp, resp_len - 1, timeout_ms);
    tls_client_close();
    if (rlen <= 0)
        return rlen < 0 ? rlen : EIO;
    resp[rlen] = '\0';
    return rlen;
}

int net_https_post_ip(const char *host, uint32_t ip, uint16_t port, const char *path,
                      const char *headers, const char *body, char *resp, int resp_len,
                      int timeout_ms) {
    if (!host || !path || !resp || resp_len <= 0 || ip == 0)
        return EINVAL;
    if (tls_client_connect_ip(host, ip, port, timeout_ms) != 0)
        return EIO;
    return https_post_send(host, path, headers, body, resp, resp_len, timeout_ms);
}

int net_https_post_port(const char *host, uint16_t port, const char *path,
                        const char *headers, const char *body, char *resp, int resp_len,
                        int timeout_ms) {
    if (!host || !path || !resp || resp_len <= 0)
        return EINVAL;
    if (tls_client_connect(host, port, timeout_ms) != 0)
        return EIO;
    return https_post_send(host, path, headers, body, resp, resp_len, timeout_ms);
}

int net_https_post(const char *host, const char *path, const char *headers,
                   const char *body, char *resp, int resp_len, int timeout_ms) {
    return net_https_post_port(host, 443, path, headers, body, resp, resp_len, timeout_ms);
}
