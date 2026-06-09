#include "fleet.h"
#include "agent.h"
#include "printf.h"
#include "ramfs.h"
#include "quota.h"
#include "policy.h"
#include "timer.h"
#include "http.h"
#include "../include/platform.h"
#include "../include/agentos.h"
#include "../include/version.h"
#include "netstack.h"

#define FLEET_CACHE "/agent/0/fleet.json"
#define FLEET_COLLECTOR_DEFAULT "http://ingest/openagentos"
#define FLEET_COLLECTOR_NET "http://10.0.2.2:8765/ingest"

static int append_ulong(char *buf, int pos, int cap, unsigned long val) {
    char tmp[16];
    int i = 0;

    if (val == 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '0';
        return pos;
    }
    while (val > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = tmp[--i];
    }
    return pos;
}

static int append_int(char *buf, int pos, int cap, int val) {
    if (val < 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '-';
        return append_ulong(buf, pos, cap, (unsigned long)(-val));
    }
    return append_ulong(buf, pos, cap, (unsigned long)val);
}

static int parse_http_url(const char *url, char *host, int hcap, char *path, int pcap,
                          uint16_t *port, int *is_https) {
    int i = 0;
    int hp = 0;
    int pp = 0;

    if (!url || !host || !path || !port || !is_https || hcap <= 0 || pcap <= 0)
        return EINVAL;
    *is_https = 0;
    if (url[0] != 'h' || url[1] != 't' || url[2] != 't' || url[3] != 'p')
        return EINVAL;
    i = 4;
    if (url[i] == 's') {
        *is_https = 1;
        i++;
        if (url[i] != ':')
            return EINVAL;
    }
    if (url[i] != ':' || url[i + 1] != '/' || url[i + 2] != '/')
        return EINVAL;
    i += 3;

    while (url[i] && url[i] != '/' && url[i] != ':') {
        if (hp >= hcap - 1)
            return ENOSPC;
        host[hp++] = url[i++];
    }
    host[hp] = '\0';
    if (!hp)
        return EINVAL;

    *port = *is_https ? 443 : 80;
    if (url[i] == ':') {
        unsigned long p = 0;
        i++;
        while (url[i] >= '0' && url[i] <= '9') {
            p = p * 10 + (unsigned long)(url[i] - '0');
            if (p > 65535)
                return EINVAL;
            i++;
        }
        *port = (uint16_t)p;
    }

    if (!url[i]) {
        path[0] = '/';
        path[1] = '\0';
        return 0;
    }
    pp = 0;
    while (url[i]) {
        if (pp >= pcap - 1)
            return ENOSPC;
        path[pp++] = url[i++];
    }
    path[pp] = '\0';
    return 0;
}

static int fleet_ingest_net(const char *url, const char *json, int json_len, char *resp,
                            int resp_len) {
    char host[64];
    char path[128];
    uint16_t port;
    int is_https = 0;
    int rc;

    (void)json_len;
    rc = parse_http_url(url, host, (int)sizeof(host), path, (int)sizeof(path), &port,
                        &is_https);
    if (rc < 0)
        return rc;
    if (policy_net_allow(host, port) != 0)
        return EPERM;
    if (is_https) {
        rc = net_https_post_port(host, port, path, 0, json, resp, resp_len, 15000);
        if (rc >= 0)
            kprintf("[fleet] ingest tls ok agent url=%s bytes=%d\n", url, rc);
        return rc;
    }
    return net_http_post(host, port, path, 0, json, resp, resp_len, 12000);
}

static int append_str(char *buf, int pos, int cap, const char *s) {
    if (!s)
        return pos;
    while (*s && pos < cap - 1)
        buf[pos++] = *s++;
    return pos;
}

void fleet_init(void) {
    if (!ramfs_path_exists(FLEET_COLLECTOR_PATH))
        ramfs_put(FLEET_COLLECTOR_PATH, FLEET_COLLECTOR_DEFAULT,
                  (int)sizeof(FLEET_COLLECTOR_DEFAULT) - 1);
    kprintf("[fleet] telemetry ready cache=%s collector=%s\n",
            FLEET_CACHE, FLEET_COLLECTOR_PATH);
}

int fleet_status(struct agent *a) {
    int used = 0;
    int max_ipc = QUOTA_IPC_MAX;

    if (a) {
        used = quota_ipc_used(a);
        max_ipc = quota_ipc_max(a);
    }
    kprintf("[fleet] platform=%s version=%s agents=%d uptime=%lu quota_ipc=%d/%d\n",
            platform_name(), OPENAGENTOS_VERSION, agent_count, timer_now(), used, max_ipc);
    return agent_count;
}

int fleet_push(struct agent *a) {
    char json[256];
    int pos = 0;
    int used = 0;
    int max_ipc = QUOTA_IPC_MAX;
    int rc;

    if (a) {
        used = quota_ipc_used(a);
        max_ipc = quota_ipc_max(a);
    }

    json[pos++] = '{';
    json[pos++] = '"';
    json[pos++] = 'v';
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '"';
    pos = append_str(json, pos, (int)sizeof(json), OPENAGENTOS_VERSION);
    if (pos < 0)
        return ENOSPC;
    json[pos++] = '"';
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'p';
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '"';
    pos = append_str(json, pos, (int)sizeof(json), platform_name());
    if (pos < 0)
        return ENOSPC;
    json[pos++] = '"';
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'a';
    json[pos++] = 'g';
    json[pos++] = 'e';
    json[pos++] = 'n';
    json[pos++] = 't';
    json[pos++] = 's';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), agent_count);
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'u';
    json[pos++] = 'p';
    json[pos++] = 't';
    json[pos++] = 'i';
    json[pos++] = 'm';
    json[pos++] = 'e';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_ulong(json, pos, (int)sizeof(json), timer_now());
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'q';
    json[pos++] = 'u';
    json[pos++] = 'o';
    json[pos++] = 't';
    json[pos++] = 'a';
    json[pos++] = '_';
    json[pos++] = 'i';
    json[pos++] = 'p';
    json[pos++] = 'c';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), used);
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'q';
    json[pos++] = 'u';
    json[pos++] = 'o';
    json[pos++] = 't';
    json[pos++] = 'a';
    json[pos++] = '_';
    json[pos++] = 'i';
    json[pos++] = 'p';
    json[pos++] = 'c';
    json[pos++] = '_';
    json[pos++] = 'm';
    json[pos++] = 'a';
    json[pos++] = 'x';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), max_ipc);
    if (pos < 0 || pos >= (int)sizeof(json) - 2)
        return ENOSPC;
    json[pos++] = '}';
    json[pos] = '\0';

    rc = ramfs_put(FLEET_CACHE, json, pos);
    if (rc < 0) {
        kprintf("[fleet] push failed rc=%d\n", rc);
        return rc;
    }
    kprintf("[fleet] push ok bytes=%d path=%s\n", pos, FLEET_CACHE);
    (void)a;
    return pos;
}

int fleet_ingest(struct agent *a, const char *url) {
    char json[RAMFS_FILE_SIZE];
    char resp[HTTP_MAX_BODY];
    char url_buf[HTTP_MAX_URL];
    int n;
    int rc;

    n = ramfs_read(FLEET_CACHE, 0, json, (int)sizeof(json) - 1);
    if (n <= 0) {
        kprintf("[fleet] ingest no cache agent=%d\n", a ? a->id : -1);
        return ENOENT;
    }
    json[n] = '\0';

    if (!url || !url[0]) {
        if (netstack_ready())
            url = FLEET_COLLECTOR_NET;
        else {
            n = ramfs_read(FLEET_COLLECTOR_PATH, 0, url_buf, (int)sizeof(url_buf) - 1);
            if (n > 0) {
                url_buf[n] = '\0';
                url = url_buf;
            } else {
                url = FLEET_COLLECTOR_DEFAULT;
            }
        }
    }

    if (netstack_ready()) {
        rc = fleet_ingest_net(url, json, n, resp, (int)sizeof(resp));
        if (rc < 0) {
            kprintf("[fleet] ingest net failed agent=%d url=%s rc=%d\n",
                    a ? a->id : -1, url, rc);
            return rc;
        }
        kprintf("[fleet] ingest net ok agent=%d url=%s bytes=%d\n",
                a ? a->id : -1, url, n);
        return rc;
    }

    rc = agent_http_fetch(url, resp, (int)sizeof(resp));
    if (rc < 0) {
        kprintf("[fleet] ingest failed agent=%d url=%s rc=%d\n",
                a ? a->id : -1, url, rc);
        return rc;
    }
    kprintf("[fleet] ingest ok agent=%d url=%s cache=%s\n",
            a ? a->id : -1, url, FLEET_CACHE);
    return rc;
}

int fleet_probe(struct agent *a, const char *url) {
    if (!url || !url[0])
        return EINVAL;
    kprintf("[fleet] probe agent=%d url=%s\n", a ? a->id : -1, url);
    return 0;
}
