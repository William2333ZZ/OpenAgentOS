#include "libagent.h"

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

void network_agent_main(void) {
    agent_log_str("[network] service ready id=");
    agent_log_int(NET_AGENT_ID);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        char req[MSG_PAYLOAD_SIZE];
        char rsp[MSG_PAYLOAD_SIZE];
        char backend[32];
        char body[HTTP_MAX_BODY];
        const char *url;
        int sender;
        int reqid;
        int n;

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

        if (str_eq(backend, "offline")) {
            format_http_rsp(rsp, reqid, "err:offline");
            agent_send_msg(sender, MSG_HTTP_RSP, rsp);
            continue;
        }

        n = agent_http(url, body, (int)sizeof(body));
        if (n < 0) {
            format_http_rsp(rsp, reqid, "err:http");
            agent_send_msg(sender, MSG_HTTP_RSP, rsp);
            continue;
        }
        body[n] = '\0';
        format_http_rsp(rsp, reqid, body);
        agent_send_msg(sender, MSG_HTTP_RSP, rsp);
    }
}
