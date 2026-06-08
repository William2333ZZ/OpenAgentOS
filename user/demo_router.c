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

static void client_worker(void) {
    char answer[LLM_MAX_RESPONSE];
    int n;

    agent_log_str("[client] svc_llm faux backend\n");
    n = agent_svc_llm("What is 17+25? Reply with only the number.", answer,
                      sizeof(answer), "faux");
    if (n < 0) {
        agent_log_str("[client] faux llm failed\n");
        agent_exit(1);
    }
    agent_log_str("[client] answer: ");
    agent_log_str(answer);
    agent_log_str("\n");
    if (!str_eq(answer, "42")) {
        agent_log_str("[client] unexpected faux answer\n");
        agent_exit(1);
    }
    agent_send_msg(1, MSG_RESULT, answer);
    agent_exit(0);
}

void init_router_agent(void) {
    struct agent_msg msg;
    char scratch[32];
    int client_id;
    int n;

    agent_log_str("[init] v3.2 model router demo\n");

    n = agent_svc_llm("ignored", scratch, sizeof(scratch), "offline");
    if (n >= 0) {
        agent_log_str("[init] offline should have failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] router offline rejected\n");

    client_id = agent_spawn(client_worker, "client",
                            CAP_LOG | CAP_SEND | CAP_RECV | CAP_TIME);
    agent_log_str("[init] spawned client id=");
    agent_log_int(client_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[init] client result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (!str_eq(msg.payload, "42")) {
        agent_log_str("[init] router faux check failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] router faux ok\n");

    agent_send_msg(ROUTER_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_log_str("[init] router demo complete\n");
    agent_exit(0);
}
