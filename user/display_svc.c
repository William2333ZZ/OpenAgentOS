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

static int parse_display_req(const char *req, int *reqid, const char **text, int *x, int *y) {
    int id = 0;
    int i = 1;
    int xi = 0;
    int yi = 0;

    *reqid = 0;
    *text = req;
    *x = 0;
    *y = 0;
    if (req[0] != 'D')
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
    while (req[i] >= '0' && req[i] <= '9')
        xi = xi * 10 + (req[i++] - '0');
    if (req[i] != ',')
        return 0;
    i++;
    while (req[i] >= '0' && req[i] <= '9')
        yi = yi * 10 + (req[i++] - '0');
    if (req[i] != '|')
        return 0;
    *reqid = id;
    *x = xi;
    *y = yi;
    *text = req + i + 1;
    return 1;
}

static void format_display_rsp(char *out, int reqid, const char *body) {
    int pos = 0;
    int i = 0;
    char idbuf[12];

    out[pos++] = 'D';
    int_to_str(reqid, idbuf);
    for (i = 0; idbuf[i]; i++)
        out[pos++] = idbuf[i];
    out[pos++] = '|';
    for (i = 0; body[i] && pos < MSG_PAYLOAD_SIZE - 1; i++)
        out[pos++] = body[i];
    out[pos] = '\0';
}

static int display_clear(void) {
    return (int)sys_agent_tool(TOOL_DISPLAY, 0, 0xff102030, 0);
}

static int display_text_at(int x, int y, const char *text) {
    long xy = ((long)(x & 0xffff) << 16) | (unsigned long)(y & 0xffff);
    if (display_clear() < 0)
        return -1;
    if ((int)sys_agent_tool(TOOL_DISPLAY, 1, xy, (long)text) < 0)
        return -1;
    (void)sys_agent_tool(TOOL_DISPLAY, 2, 0, 0);
    return 0;
}

void display_agent_main(void) {
    agent_log_str("[display] service ready id=");
    agent_log_int(DISPLAY_AGENT_ID);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        char req[MSG_PAYLOAD_SIZE];
        char rsp[MSG_PAYLOAD_SIZE];
        const char *text;
        int sender;
        int reqid;
        int x;
        int y;

        agent_recv_msg(&msg);
        sender = msg.sender_id;

        if (msg.type == MSG_PIPELINE_DONE) {
            agent_log_str("[display] shutdown\n");
            agent_exit(0);
        }

        if (msg.type != MSG_DISPLAY_REQ) {
            format_display_rsp(rsp, 0, "err:unknown");
            agent_send_msg(sender, MSG_DISPLAY_RSP, rsp);
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

        if (!parse_display_req(req, &reqid, &text, &x, &y)) {
            format_display_rsp(rsp, 0, "err:parse");
            agent_send_msg(sender, MSG_DISPLAY_RSP, rsp);
            continue;
        }

        agent_log_str("[display] draw req=");
        agent_log_int(reqid);
        agent_log_str(" text=");
        agent_log_str(text);
        agent_log_str("\n");

        if (display_text_at(x, y, text) < 0) {
            format_display_rsp(rsp, reqid, "err:draw");
            agent_send_msg(sender, MSG_DISPLAY_RSP, rsp);
            continue;
        }

        format_display_rsp(rsp, reqid, "ok");
        agent_send_msg(sender, MSG_DISPLAY_RSP, rsp);
    }
}
