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

static int key_is_launch(int code) {
    /* Linux KEY_1 and common QEMU virtio-keyboard maps. */
    return code == 2 || code == 79 || code == 49;
}

static void draw_launcher(void) {
    if (agent_svc_display("AgentOS v5.4 Desktop", 8, 8) < 0)
        return;
    agent_svc_display("Apps:", 8, 24);
    agent_svc_display("[1] worker.agent", 8, 40);
    agent_svc_display("Press 1 to launch", 8, 56);
}

static int run_desktop_shell(void) {
    struct agent_msg msg;
    int loops;
    int key = 0;

    draw_launcher();
    agent_log_str("[shell] desktop launcher ready\n");

    for (loops = 0; loops < 12000 && !key_is_launch(key); loops++) {
        key = agent_svc_input_poll();
        if (key_is_launch(key))
            break;
        agent_yield();
    }
    if (!key_is_launch(key)) {
        agent_log_str("[shell] input timeout waiting for launch key\n");
        return -1;
    }
    agent_log_str("[shell] launch key ok code=");
    agent_log_int(key);
    agent_log_str("\n");

    if (agent_send_msg(1, MSG_SHELL_REQ, "load:worker") < 0) {
        agent_log_str("[shell] load request failed\n");
        return -1;
    }
    agent_yield();

    agent_recv_msg(&msg);
    if (msg.type != MSG_SHELL_RSP || !str_eq(msg.payload, "load ok")) {
        agent_log_str("[shell] unexpected load response\n");
        return -1;
    }

    agent_svc_display("worker launched sum=42", 8, 72);
    agent_log_str("[shell] desktop demo complete\n");
    if (agent_send_msg(1, MSG_RESULT, "desktop demo complete") < 0)
        return -1;
    return 0;
}

void shell_agent_main(void) {
    agent_log_str("[shell] service ready id=");
    agent_log_int(SHELL_AGENT_ID);
    agent_log_str("\n");

    if (run_desktop_shell() < 0)
        agent_exit(1);
    agent_exit(0);
}
