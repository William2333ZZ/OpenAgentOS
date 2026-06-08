#include "libagent.h"

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
        while (body[i] && i < MSG_PAYLOAD_SIZE - 1)
            out[i++] = body[i];
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
