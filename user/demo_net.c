#include "../user/libagent.h"

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

static void client_worker(void) {
    char body[HTTP_MAX_BODY];
    int n;

    agent_log_str("[client] svc_http faux backend\n");
    n = agent_svc_http("http://stub/agentos", body, sizeof(body), "faux");
    if (n < 0) {
        agent_log_str("[client] faux http failed\n");
        agent_exit(1);
    }
    agent_log_str("[client] body: ");
    agent_log_str(body);
    agent_log_str("\n");
    if (!str_starts_with(body, "200:agentos-net-stub")) {
        agent_log_str("[client] unexpected faux body\n");
        agent_exit(1);
    }
    agent_send_msg(1, MSG_RESULT, body);
    agent_exit(0);
}

void init_net_agent(void) {
    struct agent_msg msg;
    char scratch[32];
    int client_id;
    int n;

    agent_log_str("[init] v3.4 network service demo\n");

    n = agent_svc_http("http://stub/reject", scratch, sizeof(scratch), "offline");
    if (n >= 0) {
        agent_log_str("[init] offline should have failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] network offline rejected\n");

    client_id = agent_spawn(client_worker, "client",
                            CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME);
    agent_log_str("[init] spawned client id=");
    agent_log_int(client_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[init] client result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (!str_starts_with(msg.payload, "200:agentos-net-stub")) {
        agent_log_str("[init] network faux check failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] network faux ok\n");

    agent_send_msg(NET_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_log_str("[init] net demo complete\n");
    agent_exit(0);
}
