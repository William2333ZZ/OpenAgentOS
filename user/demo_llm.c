#include "../user/libagent.h"

static void llm_worker(void) {
    char response[LLM_MAX_RESPONSE];

    agent_log_str("[llm-worker] querying DeepSeek...\n");

    int n = agent_llm("What is 17+25? Reply with only the number, no explanation.",
                      response, sizeof(response));
    if (n < 0) {
        agent_log_str("[llm-worker] query failed err=");
        agent_log_int(n);
        agent_log_str(" (timeout=-8, no-device=-6)\n");
        agent_send_msg(1, MSG_RESULT, "error");
    } else {
        agent_log_str("[llm-worker] answer: ");
        agent_log_str(response);
        agent_log_str("\n");
        agent_send_msg(1, MSG_RESULT, response);
    }
    agent_exit(0);
}

void init_llm_agent(void) {
    agent_log_str("[init] AgentOS LLM demo (native DeepSeek via VirtIO-net HTTPS)\n");

    int wid = agent_spawn(llm_worker, "llm-worker", CAP_LLM | CAP_LOG | CAP_SEND);
    agent_log_str("[init] spawned llm-worker id=");
    agent_log_int(wid);
    agent_log_str("\n");

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str("[init] pipeline complete, llm said: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_exit(0);
}
