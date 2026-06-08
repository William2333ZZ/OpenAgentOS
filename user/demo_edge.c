#include "../user/libagent.h"

#define MSG_ALERT 10

static int rules_id;
static int llm_id;
static const int init_id = 1;

static void str_cat(char *dst, const char *src, int maxlen) {
    int pos = 0;
    while (dst[pos])
        pos++;
    while (src[0] && pos < maxlen - 1) {
        dst[pos++] = src[0];
        src++;
    }
    dst[pos] = '\0';
}

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

static int parse_peak(const char *csv) {
    int peak = 0;
    int cur = 0;
    int i = 0;

    while (csv[i]) {
        if (csv[i] >= '0' && csv[i] <= '9') {
            cur = cur * 10 + (csv[i] - '0');
        } else if (csv[i] == ',') {
            if (cur > peak)
                peak = cur;
            cur = 0;
        }
        i++;
    }
    if (cur > peak)
        peak = cur;
    return peak;
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

static void sensor_worker(void) {
    agent_log_str("[sensor] writing samples 42,55,91\n");
    if (agent_svc_write("/agent/samples", "42,55,91", 8) < 0) {
        agent_log_str("[sensor] write samples failed\n");
        agent_exit(1);
    }
    agent_send_msg(rules_id, MSG_TASK, "samples-ready");
    agent_log_str("[sensor] done\n");
    agent_exit(0);
}

static void rules_worker(void) {
    struct agent_msg msg;
    char samples[64];
    char alert[32];
    int peak;

    agent_recv_msg(&msg);
    agent_log_str("[rules] task: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (agent_svc_read("/agent/samples", samples, sizeof(samples)) < 0) {
        agent_log_str("[rules] read samples failed\n");
        agent_exit(1);
    }
    agent_log_str("[rules] samples: ");
    agent_log_str(samples);
    agent_log_str("\n");

    peak = parse_peak(samples);
    agent_log_str("[rules] peak=");
    agent_log_int(peak);
    agent_log_str("\n");

    if (peak <= 80) {
        agent_log_str("[rules] within threshold\n");
        agent_send_msg(init_id, MSG_RESULT, "ok");
        agent_exit(0);
    }

    alert[0] = '\0';
    str_cat(alert, "peak=", sizeof(alert));
    int_to_str(peak, alert + str_len(alert));
    agent_log_str("[rules] alert: ");
    agent_log_str(alert);
    agent_log_str("\n");
    agent_send_msg(llm_id, MSG_ALERT, alert);
    agent_exit(0);
}

static void append_assistant_line(const char *response) {
    char line[LLM_MAX_RESPONSE + 16];
    int pos = 0;
    int i;

    line[pos++] = 'a';
    line[pos++] = 's';
    line[pos++] = 's';
    line[pos++] = 'i';
    line[pos++] = 's';
    line[pos++] = 't';
    line[pos++] = 'a';
    line[pos++] = 'n';
    line[pos++] = 't';
    line[pos++] = ':';
    line[pos++] = ' ';
    for (i = 0; response[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = response[i];
    line[pos] = '\0';
    agent_session_append(line);
}

static void llm_worker(void) {
    struct agent_msg msg;
    char prompt[LLM_MAX_PROMPT];
    char response[LLM_MAX_RESPONSE];
    int n;

    agent_recv_msg(&msg);
    agent_log_str("[llm] alert: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    prompt[0] = '\0';
    str_cat(prompt, "Edge sensor ", sizeof(prompt));
    str_cat(prompt, msg.payload, sizeof(prompt));
    str_cat(prompt, " exceeds threshold 80. Explain briefly.", sizeof(prompt));

    if (agent_session_append("user: edge-alert") < 0) {
        agent_log_str("[llm] session append failed\n");
        agent_exit(1);
    }

    agent_log_str("[llm] querying router (faux)...\n");
    n = agent_svc_llm(prompt, response, sizeof(response), "faux");
    if (n < 0) {
        agent_log_str("[llm] router llm failed\n");
        agent_exit(1);
    }
    append_assistant_line(response);
    agent_log_str("[llm] explanation: ");
    agent_log_str(response);
    agent_log_str("\n");

    if (agent_svc_write("/agent/llm_last", response, n) < 0) {
        agent_log_str("[llm] write llm_last failed\n");
        agent_exit(1);
    }

    if (agent_sync() < 0) {
        agent_log_str("[llm] sync failed\n");
        agent_exit(1);
    }
    agent_log_str("[llm] persist sync ok\n");

    agent_send_msg(init_id, MSG_RESULT, response);
    agent_exit(0);
}

static int try_restore(void) {
    char samples[64];
    char llm_last[LLM_MAX_RESPONSE];

    if (agent_svc_read("/agent/samples", samples, sizeof(samples)) < 0 ||
        !samples[0])
        return 0;
    if (agent_svc_read("/agent/llm_last", llm_last, sizeof(llm_last)) < 0 ||
        !llm_last[0])
        return 0;
    if (!str_starts_with(llm_last, "High temperature"))
        return 0;

    agent_log_str("[init] restored samples: ");
    agent_log_str(samples);
    agent_log_str("\n[init] restored llm_last: ");
    agent_log_str(llm_last);
    agent_log_str("\n");
    agent_log_str("[init] edge restore ok\n");
    return 1;
}

static void run_pipeline(void) {
    struct agent_msg msg;
    int sensor_id;

    rules_id = agent_spawn(rules_worker, "rules",
                           CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME);
    llm_id = agent_spawn(llm_worker, "llm-worker",
                         CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME);
    sensor_id = agent_spawn(sensor_worker, "sensor",
                            CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME);

    agent_log_str("[init] spawned sensor=");
    agent_log_int(sensor_id);
    agent_log_str(" rules=");
    agent_log_int(rules_id);
    agent_log_str(" llm=");
    agent_log_int(llm_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[init] pipeline result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (!str_starts_with(msg.payload, "High temperature")) {
        agent_log_str("[init] unexpected llm result\n");
        agent_exit(1);
    }
}

void init_edge_agent(void) {
    agent_log_str("[init] v3.2 edge: sensor -> rules -> router -> persist\n");

    if (try_restore()) {
        agent_send_msg(ROUTER_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
        agent_send_msg(STORAGE_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
        agent_log_str("[init] edge demo complete\n");
        agent_exit(0);
    }

    run_pipeline();
    agent_send_msg(ROUTER_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_send_msg(STORAGE_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_log_str("[init] edge demo complete\n");
    agent_exit(0);
}

static void snapshot_msg(struct agent_msg *msg, int *sender, int *mtype, char *req) {
    int i;

    *sender = msg->sender_id;
    *mtype = msg->type;
    for (i = 0; i < MSG_PAYLOAD_SIZE - 1; i++) {
        req[i] = msg->payload[i];
        if (!msg->payload[i])
            break;
    }
    req[i] = '\0';
}

static int parse_req_id(const char *req, int *reqid, const char **rest) {
    int id = 0;
    int i = 0;

    *reqid = 0;
    *rest = req;
    if (req[0] != 'R' && req[0] != 'W')
        return 0;
    i = 1;
    if (req[i] < '0' || req[i] > '9')
        return 0;
    while (req[i] >= '0' && req[i] <= '9') {
        id = id * 10 + (req[i] - '0');
        i++;
    }
    if (req[i] != '|')
        return 0;
    *reqid = id;
    *rest = req + i + 1;
    return 1;
}

static void format_fs_rsp(char *out, int reqid, const char *body) {
    int pos = 0;
    int i = 0;
    char idbuf[12];

    if (reqid <= 0) {
        for (i = 0; body[i] && i < MSG_PAYLOAD_SIZE - 1; i++)
            out[i] = body[i];
        out[i] = '\0';
        return;
    }

    out[pos++] = 'R';
    int_to_str(reqid, idbuf);
    for (i = 0; idbuf[i]; i++)
        out[pos++] = idbuf[i];
    out[pos++] = '|';
    for (i = 0; body[i] && pos < MSG_PAYLOAD_SIZE - 1; i++)
        out[pos++] = body[i];
    out[pos] = '\0';
}

void storage_agent_main(void) {
    agent_log_str("[storage] service ready id=");
    agent_log_int(STORAGE_AGENT_ID);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        int sender;
        int mtype;
        char req[MSG_PAYLOAD_SIZE];
        char buf[200];
        char rsp[MSG_PAYLOAD_SIZE];
        int reqid;
        const char *body;
        int i;
        int n;

        agent_recv_msg(&msg);
        snapshot_msg(&msg, &sender, &mtype, req);

        if (mtype == MSG_PIPELINE_DONE) {
            agent_log_str("[storage] shutdown\n");
            agent_exit(0);
        }

        if (mtype == MSG_FS_READ) {
            char path[64];
            int pi = 0;

            if (!parse_req_id(req, &reqid, &body))
                body = req;

            while (body[pi] && pi < 63) {
                path[pi] = body[pi];
                pi++;
            }
            path[pi] = '\0';

            n = agent_read_file(path, buf, sizeof(buf) - 1);
            if (n < 0)
                format_fs_rsp(rsp, reqid, "err:read");
            else {
                buf[n] = '\0';
                format_fs_rsp(rsp, reqid, buf);
            }
            agent_send_msg(sender, MSG_FS_RSP, rsp);
            continue;
        }

        if (mtype == MSG_FS_WRITE) {
            char path[64];
            char data[128];
            const char *sep;
            int pi = 0;
            int di = 0;

            if (!parse_req_id(req, &reqid, &body))
                body = req;

            sep = 0;
            for (i = 0; body[i]; i++) {
                if (body[i] == '|') {
                    sep = body + i;
                    break;
                }
            }
            if (!sep) {
                format_fs_rsp(rsp, reqid, "err:write");
                agent_send_msg(sender, MSG_FS_RSP, rsp);
                continue;
            }
            while (body + pi < sep && pi < 63) {
                path[pi] = body[pi];
                pi++;
            }
            path[pi] = '\0';
            for (const char *p = sep + 1; *p && di < 127; p++)
                data[di++] = *p;
            data[di] = '\0';

            if (agent_write_file(path, data, di) < 0)
                format_fs_rsp(rsp, reqid, "err:write");
            else
                format_fs_rsp(rsp, reqid, "ok");
            agent_send_msg(sender, MSG_FS_RSP, rsp);
            continue;
        }

        format_fs_rsp(rsp, 0, "err:unknown");
        agent_send_msg(sender, MSG_FS_RSP, rsp);
    }
}
