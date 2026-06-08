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

static void format_input_rsp(char *out, int reqid, int code) {
    int pos = 0;
    int i = 0;
    char idbuf[12];
    char codebuf[12];

    out[pos++] = 'I';
    int_to_str(reqid, idbuf);
    for (i = 0; idbuf[i]; i++)
        out[pos++] = idbuf[i];
    out[pos++] = '|';
    if (code < 0) {
        out[pos++] = 'e';
        out[pos++] = 'r';
        out[pos++] = 'r';
    } else if (code == 0) {
        out[pos++] = 'n';
        out[pos++] = 'o';
        out[pos++] = 'n';
        out[pos++] = 'e';
    } else {
        out[pos++] = 'k';
        out[pos++] = 'e';
        out[pos++] = 'y';
        out[pos++] = '|';
        int_to_str(code, codebuf);
        for (i = 0; codebuf[i] && pos < MSG_PAYLOAD_SIZE - 1; i++)
            out[pos++] = codebuf[i];
    }
    out[pos] = '\0';
}

void input_agent_main(void) {
    agent_log_str("[input] service ready id=");
    agent_log_int(INPUT_AGENT_ID);
    agent_log_str("\n");

    for (;;) {
        struct agent_msg msg;
        char req[MSG_PAYLOAD_SIZE];
        char rsp[MSG_PAYLOAD_SIZE];
        int sender;
        int reqid = 0;
        int code;
        int i;

        agent_recv_msg(&msg);
        sender = msg.sender_id;

        if (msg.type == MSG_PIPELINE_DONE) {
            agent_log_str("[input] shutdown\n");
            agent_exit(0);
        }

        if (msg.type != MSG_INPUT_REQ) {
            format_input_rsp(rsp, 0, -1);
            agent_send_msg(sender, MSG_INPUT_RSP, rsp);
            continue;
        }

        for (i = 0; i < MSG_PAYLOAD_SIZE - 1; i++) {
            req[i] = msg.payload[i];
            if (!msg.payload[i])
                break;
        }
        req[i] = '\0';

        if (req[0] == 'I' && req[1] >= '0' && req[1] <= '9') {
            i = 1;
            while (req[i] >= '0' && req[i] <= '9')
                reqid = reqid * 10 + (req[i++] - '0');
        }

        code = (int)sys_agent_tool(TOOL_INPUT, 0, 0, 0);
        if (code > 0) {
            agent_log_str("[input] key down code=");
            agent_log_int(code);
            agent_log_str("\n");
        }

        format_input_rsp(rsp, reqid, code);
        agent_send_msg(sender, MSG_INPUT_RSP, rsp);
    }
}
