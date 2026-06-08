#include "../user/libagent.h"

#define UI_KEY_H 11
/* QEMU monitor sendkey may surface as Linux KEY_H (11) or alternate maps. */
static int ui_key_accepted(int code) {
    return code == UI_KEY_H || code == 35 || code == 104;
}

void init_ui_agent(void) {
    int loops;
    int key = 0;

    agent_log_str("[init] v4.4 human interface demo\n");

    if (agent_svc_display("AgentOS v4.4", 8, 8) < 0) {
        agent_log_str("[init] display failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] display draw ok\n");

    for (loops = 0; loops < 8000 && !ui_key_accepted(key); loops++) {
        key = agent_svc_input_poll();
        if (ui_key_accepted(key))
            break;
        agent_yield();
    }

    if (!ui_key_accepted(key)) {
        agent_log_str("[init] input timeout waiting for key H\n");
        agent_exit(1);
    }
    agent_log_str("[init] input key ok code=");
    agent_log_int(key);
    agent_log_str("\n");

    if (agent_svc_display("Key OK", 8, 24) < 0) {
        agent_log_str("[init] display update failed\n");
        agent_exit(1);
    }

    agent_log_str("[init] ui demo complete\n");
    agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
    agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
    agent_exit(0);
}
