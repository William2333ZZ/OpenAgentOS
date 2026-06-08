#include "libagent.h"

#define NET_CACHE_DIR   "/agent/7/net-cache/"
#define PROD_QUOTA_MAX  4

static int prod_quota_used;

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static void int_to_str(int v, char *out) {
    int i = 0;
    char tmp[12];
    unsigned int n;
    int pos = 0;

    if (v < 0) {
        out[pos++] = '-';
        n = (unsigned int)(-v);
    } else {
        n = (unsigned int)v;
    }
    if (n == 0) {
        out[pos++] = '0';
        out[pos] = '\0';
        return;
    }
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        out[pos++] = tmp[--i];
    out[pos] = '\0';
}

static int parse_http_req(const char *req, int *reqid, char *backend, int backend_len,
                          const char **url) {
    int id = 0;
    int i = 1;
    int bi = 0;

    *reqid = 0;
    *url = req;
    backend[0] = '\0';
    if (req[0] != 'H')
        return 0;
    if (req[i] < '0' || req[i] > '9')
        return 0;
    while (req[i] >= '0' && req[i] <= '9') {
        id = id * 10 + (req[i] - '0');
        i++;
    }
    if (req[i] != '|')
        return 0;
    i++;
    while (req[i] && req[i] != '|' && bi < backend_len - 1)
        backend[bi++] = req[i++];
    backend[bi] = '\0';
    if (req[i] != '|')
        return 0;
    *reqid = id;
    *url = req + i + 1;
    return 1;
}

static void format_http_rsp(char *out, int reqid, const char *body) {
    int pos = 0;
    int i = 0;
    char idbuf[12];

    out[pos++] = 'H';
    int_to_str(reqid, idbuf);
    for (i = 0; idbuf[i]; i++)
        out[pos++] = idbuf[i];
    out[pos++] = '|';
    for (i = 0; body[i] && pos < MSG_PAYLOAD_SIZE - 1; i++)
        out[pos++] = body[i];
    out[pos] = '\0';
}

static int url_to_cache_path(const char *url, char *path, int path_len) {
    int pos = 0;
    int i = 0;

    for (i = 0; NET_CACHE_DIR[i] && pos < path_len - 1; i++)
        path[pos++] = NET_CACHE_DIR[i];
    for (i = 0; url[i] && pos < path_len - 5; i++) {
        char c = url[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
            path[pos++] = c;
        else
            path[pos++] = '_';
    }
    if (pos + 4 >= path_len)
        return -1;
    path[pos++] = '.';
    path[pos++] = 't';
    path[pos++] = 'x';
    path[pos++] = 't';
    path[pos] = '\0';
    return pos;
}

static int cache_read(const char *url, char *body, int body_len) {
    char path[96];
    int n;

    if (url_to_cache_path(url, path, (int)sizeof(path)) < 0)
        return -1;
    n = agent_read_file(path, body, body_len);
    if (n <= 0)
        return -1;
    body[n] = '\0';
    agent_log_str("[network] cache hit url=");
    agent_log_str(url);
    agent_log_str("\n");
    return n;
}

static int cache_write(const char *url, const char *body, int body_len) {
    char path[96];
    int n;

    if (url_to_cache_path(url, path, (int)sizeof(path)) < 0)
        return -1;
    n = agent_write_file(path, body, body_len);
    if (n < 0)
        return n;
    agent_log_str("[network] cache store url=");
    agent_log_str(url);
    agent_log_str("\n");
    return n;
}

static void seed_offline_cache(void) {
    const char *url = "http://10.0.2.2:8080/reject-degrade";
    const char *body = "cache:200:offline-ok";
    char path[96];

    if (url_to_cache_path(url, path, (int)sizeof(path)) < 0)
        return;
    if (agent_write_file(path, body, (int)str_len(body)) >= 0) {
        agent_log_str("[network] seeded offline cache for reject-degrade\n");
    }
}

static int fetch_live(const char *url, char *body, int body_len) {
    int n = agent_http(url, body, body_len);
    if (n < 0)
        return n;
    body[n] = '\0';
    return n;
}

static int handle_prod(const char *url, char *body, int body_len) {
    int n;

    if (prod_quota_used >= PROD_QUOTA_MAX) {
        agent_log_str("[network] quota exceeded\n");
        return -1;
    }
    prod_quota_used++;
    agent_log_str("[network] quota used=");
    agent_log_int(prod_quota_used);
    agent_log_str("\n");

    n = fetch_live(url, body, body_len);
    if (n >= 0) {
        cache_write(url, body, n);
        return n;
    }

    agent_log_str("[network] live fetch failed, trying cache\n");
    n = cache_read(url, body, body_len);
    if (n >= 0) {
        agent_log_str("[network] fallback cache ok\n");
        return n;
    }
    return -1;
}

static void dispatch_http(int sender, int reqid, const char *backend, const char *url,
                          char *rsp) {
    char body[HTTP_MAX_BODY];
    int n;

    if (str_eq(backend, "offline")) {
        format_http_rsp(rsp, reqid, "err:offline");
        agent_send_msg(sender, MSG_HTTP_RSP, rsp);
        return;
    }

    if (str_eq(backend, "prod")) {
        n = handle_prod(url, body, (int)sizeof(body));
        if (n < 0) {
            if (prod_quota_used >= PROD_QUOTA_MAX)
                format_http_rsp(rsp, reqid, "err:quota");
            else
                format_http_rsp(rsp, reqid, "err:offline");
            agent_send_msg(sender, MSG_HTTP_RSP, rsp);
            return;
        }
        format_http_rsp(rsp, reqid, body);
        agent_send_msg(sender, MSG_HTTP_RSP, rsp);
        return;
    }

    n = fetch_live(url, body, (int)sizeof(body));
    if (n < 0) {
        format_http_rsp(rsp, reqid, "err:http");
        agent_send_msg(sender, MSG_HTTP_RSP, rsp);
        return;
    }
    format_http_rsp(rsp, reqid, body);
    agent_send_msg(sender, MSG_HTTP_RSP, rsp);
}

void network_prod_agent_main(void) {
    prod_quota_used = 0;
    seed_offline_cache();

    agent_log_str("[network] prod service ready id=");
    agent_log_int(NET_AGENT_ID);
    agent_log_str(" quota=");
    agent_log_int(PROD_QUOTA_MAX);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        char req[MSG_PAYLOAD_SIZE];
        char rsp[MSG_PAYLOAD_SIZE];
        char backend[32];
        const char *url;
        int sender;
        int reqid;

        agent_recv_msg(&msg);
        sender = msg.sender_id;

        if (msg.type == MSG_PIPELINE_DONE) {
            agent_log_str("[network] shutdown\n");
            agent_exit(0);
        }

        if (msg.type != MSG_HTTP_REQ) {
            format_http_rsp(rsp, 0, "err:unknown");
            agent_send_msg(sender, MSG_HTTP_RSP, rsp);
            continue;
        }

        {
            int i;
            for (i = 0; i < MSG_PAYLOAD_SIZE - 1; i++) {
                req[i] = msg.payload[i];
                if (!msg.payload[i])
                    break;
            }
            req[i] = '\0';
        }

        if (!parse_http_req(req, &reqid, backend, (int)sizeof(backend), &url)) {
            format_http_rsp(rsp, 0, "err:parse");
            agent_send_msg(sender, MSG_HTTP_RSP, rsp);
            continue;
        }

        agent_log_str("[network] backend=");
        agent_log_str(backend);
        agent_log_str(" req=");
        agent_log_int(reqid);
        agent_log_str("\n");

        dispatch_http(sender, reqid, backend, url, rsp);
    }
}
