#include "../user/libagent.h"

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

void init_console_quota_agent(void) {
    struct agent_msg msg;

    agent_log_str("[init] v6.3 resource quota demo\n");
    agent_log_str("[init] waiting for console quota commands ...\n");

    for (;;) {
        agent_recv_msg(&msg);
        if (msg.type == MSG_RESULT &&
            str_eq(msg.payload, "console demo quota complete")) {
            agent_log_str("[init] console demo quota complete\n");
            agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }
    }
}
