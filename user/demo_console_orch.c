#include "../user/libagent.h"
#include "orch_pipeline.h"

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

void init_console_orch_agent(void) {
    struct agent_msg msg;

    agent_log_str("[init] v5.2 console orch demo\n");
    agent_log_str("[init] waiting for console /run orch ...\n");

    for (;;) {
        agent_recv_msg(&msg);

        if (msg.type == MSG_CONSOLE_REQ && str_eq(msg.payload, "run:orch")) {
            agent_log_str("[init] orch run requested\n");
            if (orch_run_pipeline() < 0) {
                agent_log_str("[init] orch pipeline failed\n");
                agent_exit(1);
            }
            agent_log_str("[init] orch pipeline ok\n");
            continue;
        }

        if (msg.type == MSG_RESULT && str_eq(msg.payload, "console demo orch complete")) {
            agent_log_str("[init] console demo orch complete\n");
            agent_send_msg(STORAGE_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }

        agent_log_str("[init] ignored console message type=");
        agent_log_int(msg.type);
        agent_log_str("\n");
    }
}
