#include "libagent.h"

static int svc_read_seq;
static int svc_write_seq;
static volatile int svc_read_pending_id;
static volatile int svc_write_pending_id;
static struct agent_msg svc_ipc_msg;

static struct agent_msg svc_deferred;
static int svc_have_deferred;

static int svc_recv_match(int expect_type, int (*match_id)(const char *, int, const char **),
                          int expect_id, const char **body) {
    int mtype;

    for (;;) {
        if (svc_have_deferred) {
            svc_ipc_msg = svc_deferred;
            svc_have_deferred = 0;
        } else {
            mtype = agent_recv_msg(&svc_ipc_msg);
            if (mtype < 0)
                return -1;
        }
        mtype = svc_ipc_msg.type;
        if (mtype != expect_type) {
            svc_deferred = svc_ipc_msg;
            svc_have_deferred = 1;
            agent_yield();
            continue;
        }
        if (!match_id(svc_ipc_msg.payload, expect_id, body)) {
            svc_deferred = svc_ipc_msg;
            svc_have_deferred = 1;
            agent_yield();
            continue;
        }
        return 0;
    }
}

int agent_svc_read(const char *path, char *buf, int len) {
    char req[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;
    int n;

    reqid = ++svc_read_seq;
    svc_read_pending_id = reqid;
    req[pos++] = 'R';
    pos = svc_append_int(req, pos, (int)sizeof(req), reqid);
    if (pos < 0)
        return -1;
    if (pos >= (int)sizeof(req) - 2)
        return -1;
    req[pos++] = '|';
    for (i = 0; path[i] && pos < (int)sizeof(req) - 1; i++)
        req[pos++] = path[i];
    req[pos] = '\0';

    if (agent_send_msg(STORAGE_AGENT_ID, MSG_FS_READ, req) != 0)
        return -1;

    if (svc_recv_match(MSG_FS_RSP, svc_parse_rsp_id, svc_read_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    n = str_len(body);
    if (n >= len)
        n = len - 1;
    for (i = 0; i < n; i++)
        buf[i] = body[i];
    buf[n] = '\0';
    return n;
}

int agent_svc_write(const char *path, const char *data, int len) {
    char payload[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;

    reqid = ++svc_write_seq;
    svc_write_pending_id = reqid;
    payload[pos++] = 'W';
    pos = svc_append_int(payload, pos, (int)sizeof(payload), reqid);
    if (pos < 0)
        return -1;
    if (pos >= (int)sizeof(payload) - len - 3)
        return -1;
    payload[pos++] = '|';
    for (i = 0; path[i] && pos < (int)sizeof(payload) - len - 2; i++)
        payload[pos++] = path[i];
    if (pos >= (int)sizeof(payload) - len - 2)
        return -1;
    payload[pos++] = '|';
    for (i = 0; i < len && pos < (int)sizeof(payload) - 1; i++)
        payload[pos++] = data[i];
    payload[pos] = '\0';

    if (agent_send_msg(STORAGE_AGENT_ID, MSG_FS_WRITE, payload) != 0)
        return -1;

    if (svc_recv_match(MSG_FS_RSP, svc_parse_rsp_id, svc_write_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    return len;
}

static int svc_llm_seq;
static volatile int svc_llm_pending_id;

static int svc_parse_llm_rsp(const char *payload, int expect_id, const char **body) {
    int id = 0;
    int i = 1;

    *body = payload;
    if (payload[0] != 'L')
        return 0;
    if (payload[1] < '0' || payload[1] > '9')
        return 0;
    while (payload[i] >= '0' && payload[i] <= '9') {
        id = id * 10 + (payload[i] - '0');
        i++;
    }
    if (payload[i] != '|')
        return 0;
    *body = payload + i + 1;
    return id == expect_id;
}

int agent_svc_llm(const char *prompt, char *buf, int len, const char *backend) {
    char req[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;
    int n;
    int plen;
    int blen;

    if (!prompt || !buf || len <= 0 || !backend)
        return -1;

    plen = str_len(prompt);
    blen = str_len(backend);
    if (blen <= 0 || blen > 16)
        return -1;

    reqid = ++svc_llm_seq;
    svc_llm_pending_id = reqid;
    req[pos++] = 'L';
    pos = svc_append_int(req, pos, (int)sizeof(req), reqid);
    if (pos < 0)
        return -1;
    if (pos + blen + plen + 3 >= (int)sizeof(req))
        return -1;
    req[pos++] = '|';
    for (i = 0; backend[i]; i++)
        req[pos++] = backend[i];
    req[pos++] = '|';
    for (i = 0; i < plen && pos < (int)sizeof(req) - 1; i++)
        req[pos++] = prompt[i];
    req[pos] = '\0';

    if (agent_send_msg(ROUTER_AGENT_ID, MSG_LLM_REQ, req) != 0)
        return -1;

    if (svc_recv_match(MSG_LLM_RSP, svc_parse_llm_rsp, svc_llm_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    n = str_len(body);
    if (n >= len)
        n = len - 1;
    for (i = 0; i < n; i++)
        buf[i] = body[i];
    buf[n] = '\0';
    return n;
}

static int svc_http_seq;
static volatile int svc_http_pending_id;

static int svc_parse_http_rsp(const char *payload, int expect_id, const char **body) {
    int id = 0;
    int i = 1;

    *body = payload;
    if (payload[0] != 'H')
        return 0;
    if (payload[1] < '0' || payload[1] > '9')
        return 0;
    while (payload[i] >= '0' && payload[i] <= '9') {
        id = id * 10 + (payload[i] - '0');
        i++;
    }
    if (payload[i] != '|')
        return 0;
    *body = payload + i + 1;
    return id == expect_id;
}

int agent_svc_http(const char *url, char *buf, int len, const char *backend) {
    char req[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;
    int n;
    int ulen;
    int blen;

    if (!url || !buf || len <= 0 || !backend)
        return -1;

    ulen = str_len(url);
    blen = str_len(backend);
    if (ulen <= 0 || blen <= 0 || blen > 16)
        return -1;

    reqid = ++svc_http_seq;
    svc_http_pending_id = reqid;
    req[pos++] = 'H';
    pos = svc_append_int(req, pos, (int)sizeof(req), reqid);
    if (pos < 0)
        return -1;
    if (pos + blen + ulen + 3 >= (int)sizeof(req))
        return -1;
    req[pos++] = '|';
    for (i = 0; backend[i]; i++)
        req[pos++] = backend[i];
    req[pos++] = '|';
    for (i = 0; i < ulen && pos < (int)sizeof(req) - 1; i++)
        req[pos++] = url[i];
    req[pos] = '\0';

    if (agent_send_msg(NET_AGENT_ID, MSG_HTTP_REQ, req) != 0)
        return -1;

    if (svc_recv_match(MSG_HTTP_RSP, svc_parse_http_rsp, svc_http_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    n = str_len(body);
    if (n >= len)
        n = len - 1;
    for (i = 0; i < n; i++)
        buf[i] = body[i];
    buf[n] = '\0';
    return n;
}

static int svc_display_seq;
static volatile int svc_display_pending_id;

static int svc_parse_display_rsp(const char *payload, int expect_id, const char **body) {
    int id = 0;
    int i = 1;

    *body = payload;
    if (payload[0] != 'D')
        return 0;
    if (payload[1] < '0' || payload[1] > '9')
        return 0;
    while (payload[i] >= '0' && payload[i] <= '9') {
        id = id * 10 + (payload[i] - '0');
        i++;
    }
    if (payload[i] != '|')
        return 0;
    *body = payload + i + 1;
    return id == expect_id;
}

int agent_svc_display(const char *text, int x, int y) {
    char req[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;
    int tlen;

    if (!text)
        return -1;
    tlen = str_len(text);
    if (tlen <= 0)
        return -1;

    reqid = ++svc_display_seq;
    svc_display_pending_id = reqid;
    req[pos++] = 'D';
    pos = svc_append_int(req, pos, (int)sizeof(req), reqid);
    if (pos < 0)
        return -1;
    req[pos++] = '|';
    pos = svc_append_int(req, pos, (int)sizeof(req), x);
    if (pos < 0)
        return -1;
    req[pos++] = ',';
    pos = svc_append_int(req, pos, (int)sizeof(req), y);
    if (pos < 0)
        return -1;
    req[pos++] = '|';
    if (pos + tlen >= (int)sizeof(req))
        return -1;
    for (i = 0; i < tlen && pos < (int)sizeof(req) - 1; i++)
        req[pos++] = text[i];
    req[pos] = '\0';

    if (agent_send_msg(DISPLAY_AGENT_ID, MSG_DISPLAY_REQ, req) != 0)
        return -1;
    if (svc_recv_match(MSG_DISPLAY_RSP, svc_parse_display_rsp, svc_display_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    return 0;
}

static int svc_input_seq;
static volatile int svc_input_pending_id;

static int svc_parse_input_rsp(const char *payload, int expect_id, const char **body) {
    int id = 0;
    int i = 1;

    *body = payload;
    if (payload[0] != 'I')
        return 0;
    if (payload[1] < '0' || payload[1] > '9')
        return 0;
    while (payload[i] >= '0' && payload[i] <= '9') {
        id = id * 10 + (payload[i] - '0');
        i++;
    }
    if (payload[i] != '|')
        return 0;
    *body = payload + i + 1;
    return id == expect_id;
}

int agent_svc_input_poll(void) {
    char req[MSG_PAYLOAD_SIZE];
    const char *body;
    int reqid;
    int pos = 0;
    int i;
    int code = 0;

    reqid = ++svc_input_seq;
    svc_input_pending_id = reqid;
    req[pos++] = 'I';
    pos = svc_append_int(req, pos, (int)sizeof(req), reqid);
    if (pos < 0)
        return -1;
    req[pos++] = '|';
    req[pos++] = 'p';
    req[pos++] = 'o';
    req[pos++] = 'l';
    req[pos++] = 'l';
    req[pos] = '\0';

    if (agent_send_msg(INPUT_AGENT_ID, MSG_INPUT_REQ, req) != 0)
        return -1;
    if (svc_recv_match(MSG_INPUT_RSP, svc_parse_input_rsp, svc_input_pending_id, &body) < 0)
        return -1;
    if (body[0] == 'e' && body[1] == 'r' && body[2] == 'r')
        return -1;
    if (body[0] == 'n' && body[1] == 'o')
        return 0;
    if (body[0] != 'k' || body[1] != 'e' || body[2] != 'y' || body[3] != '|')
        return -1;
    i = 4;
    while (body[i] >= '0' && body[i] <= '9') {
        code = code * 10 + (body[i] - '0');
        i++;
    }
    return code;
}
