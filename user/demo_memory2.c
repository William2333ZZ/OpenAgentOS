#include "../user/libagent.h"

static void str_copy(char *dst, const char *src, int maxlen) {
    int i = 0;
    while (src[i] && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

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

static void memory2_worker(void) {
    char version[64];
    char prompt[LLM_MAX_PROMPT];
    char response[LLM_MAX_RESPONSE];
    char session_buf[256];
    char session_line[128];
    char summary[128];
    char steer[128];
    struct agent_msg msg;
    int n;

    agent_recv_msg(&msg);
    if (msg.type != MSG_STEER) {
        agent_log_str("[memory2] expected steer first\n");
        agent_exit(1);
    }
    str_copy(steer, msg.payload, sizeof(steer));
    agent_log_str("[memory2] got steer: ");
    agent_log_str(steer);
    agent_log_str("\n");

    agent_log_str("[memory2] reading /sys/version via storage\n");
    if (agent_svc_read("/sys/version", version, sizeof(version)) < 0) {
        agent_log_str("[memory2] read /sys/version failed\n");
        agent_exit(1);
    }
    agent_log_str("[memory2] version: ");
    agent_log_str(version);
    agent_log_str("\n");

    if (agent_session_tail(session_line, sizeof(session_line)) >= 0 &&
        session_line[0]) {
        agent_log_str("[memory2] restored prior session tail: ");
        agent_log_str(session_line);
        agent_log_str("\n");
    }

    agent_session_append("user: turn1");
    agent_session_append("assistant: noted");
    agent_session_append("user: turn2");
    agent_session_append("assistant: ready");

    if (agent_session_compact(2) < 0) {
        agent_log_str("[memory2] session compact failed\n");
        agent_exit(1);
    }
    agent_log_str("[memory2] session compact ok\n");

    agent_recv_msg(&msg);
    if (msg.type != MSG_SUMMARY) {
        agent_log_str("[memory2] expected MSG_SUMMARY\n");
        agent_exit(1);
    }
    str_copy(summary, msg.payload, sizeof(summary));
    agent_log_str("[memory2] got MSG_SUMMARY: ");
    agent_log_str(summary);
    agent_log_str("\n");

    if (agent_session_read(session_buf, sizeof(session_buf), 3) < 0) {
        agent_log_str("[memory2] session read failed\n");
        agent_exit(1);
    }
    agent_log_str("[memory2] session context: ");
    agent_log_str(session_buf);
    agent_log_str("\n");

    if (!str_starts_with(session_buf, "summary:")) {
        agent_log_str("[memory2] missing summary line in session\n");
        agent_exit(1);
    }

    prompt[0] = '\0';
    str_cat(prompt, "Context: ", sizeof(prompt));
    str_cat(prompt, session_buf, sizeof(prompt));
    str_cat(prompt, " Steer: ", sizeof(prompt));
    str_cat(prompt, steer, sizeof(prompt));
    str_cat(prompt, " Version: ", sizeof(prompt));
    str_cat(prompt, version, sizeof(prompt));
    str_cat(prompt, ". What is 17+25? Reply with only the number.",
            sizeof(prompt));

    agent_session_append("user: memory2-query");
    agent_log_str("[memory2] querying LLM (streaming)...\n");
    n = agent_llm(prompt, response, sizeof(response));
    if (n < 0) {
        agent_log_str("[memory2] llm failed\n");
        agent_exit(1);
    }
    agent_log_str("[memory2] llm answer: ");
    agent_log_str(response);
    agent_log_str("\n");

    if (agent_sync() < 0) {
        agent_log_str("[memory2] sync failed\n");
        agent_exit(1);
    }
    agent_log_str("[memory2] persist sync ok\n");

    agent_send_msg(1, MSG_RESULT, response);
    agent_exit(0);
}

void init_memory2_agent(void) {
    agent_log_str("[init] v2.0 memory2 session compact + LLM stream demo\n");

    int wid = agent_spawn(memory2_worker, "memory-worker",
                          CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME | CAP_LLM);
    agent_log_str("[init] spawned memory-worker id=");
    agent_log_int(wid);
    agent_log_str("\n");

    agent_send_steering(wid, "topic=17+25");
    agent_log_str("[init] sent steer to worker\n");

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str("[init] memory2 worker answer: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    agent_send_msg(STORAGE_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_log_str("[init] memory2 demo complete\n");
    agent_exit(0);
}

static void __attribute__((noinline))
snapshot_msg(struct agent_msg *msg, int *sender, int *mtype, char *req) {
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
