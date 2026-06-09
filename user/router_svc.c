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

static int parse_llm_req(const char *req, int *reqid, char *backend, int backend_len,
                         const char **prompt) {
    int id = 0;
    int i = 1;
    int bi = 0;

    *reqid = 0;
    *prompt = req;
    backend[0] = '\0';
    if (req[0] != 'L')
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
    *prompt = req + i + 1;
    return 1;
}

static void format_llm_rsp(char *out, int reqid, const char *body) {
    int pos = 0;
    int i = 0;
    char idbuf[12];

    out[pos++] = 'L';
    int_to_str(reqid, idbuf);
    for (i = 0; idbuf[i]; i++)
        out[pos++] = idbuf[i];
    out[pos++] = '|';
    for (i = 0; body[i] && pos < MSG_PAYLOAD_SIZE - 1; i++)
        out[pos++] = body[i];
    out[pos] = '\0';
}

static int contains_substr(const char *hay, const char *needle) {
    int i = 0;
    int j;
    if (!hay || !needle || !needle[0])
        return 0;
    while (hay[i]) {
        j = 0;
        while (needle[j] && hay[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
        i++;
    }
    return 0;
}

static int router_faux(const char *prompt, char *answer, int answer_len) {
    const char *body = "ok";
    int len = 2;
    int i;

    if (!prompt || !answer || answer_len <= 0)
        return -1;
    if (contains_substr(prompt, "17+25"))
        body = "42";
    else if (contains_substr(prompt, "2+2"))
        body = "4";
    len = 0;
    while (body[len])
        len++;
    if (len >= answer_len)
        return -1;
    for (i = 0; i <= len; i++)
        answer[i] = body[i];
    return len;
}

void router_agent_main(void) {
    agent_log_str("[router] service ready id=");
    agent_log_int(ROUTER_AGENT_ID);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        char req[MSG_PAYLOAD_SIZE];
        char rsp[MSG_PAYLOAD_SIZE];
        char backend[32];
        char answer[LLM_MAX_RESPONSE];
        const char *prompt;
        int sender;
        int reqid;
        int n;

        agent_recv_msg(&msg);
        sender = msg.sender_id;

        if (msg.type == MSG_PIPELINE_DONE) {
            agent_log_str("[router] shutdown\n");
            agent_exit(0);
        }

        if (msg.type != MSG_LLM_REQ) {
            format_llm_rsp(rsp, 0, "err:unknown");
            agent_send_msg(sender, MSG_LLM_RSP, rsp);
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

        if (!parse_llm_req(req, &reqid, backend, (int)sizeof(backend), &prompt)) {
            format_llm_rsp(rsp, 0, "err:parse");
            agent_send_msg(sender, MSG_LLM_RSP, rsp);
            continue;
        }

        agent_log_str("[router] backend=");
        agent_log_str(backend);
        agent_log_str(" req=");
        agent_log_int(reqid);
        agent_log_str("\n");

        if (str_eq(backend, "offline")) {
            format_llm_rsp(rsp, reqid, "err:offline");
            agent_send_msg(sender, MSG_LLM_RSP, rsp);
            continue;
        }

        if (str_eq(backend, "faux")) {
            n = router_faux(prompt, answer, (int)sizeof(answer));
        } else {
            n = agent_llm(prompt, answer, (int)sizeof(answer));
        }
        if (n < 0) {
            format_llm_rsp(rsp, reqid, "err:llm");
            agent_send_msg(sender, MSG_LLM_RSP, rsp);
            continue;
        }
        answer[n] = '\0';
        format_llm_rsp(rsp, reqid, answer);
        agent_send_msg(sender, MSG_LLM_RSP, rsp);
    }
}
