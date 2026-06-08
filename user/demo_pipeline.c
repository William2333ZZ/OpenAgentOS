#include "../user/libagent.h"

static int worker_id;
static int planner_id;

static void worker_agent(void) {
    struct agent_msg msg;
    agent_log_str("[worker] started\n");

    agent_recv_msg(&msg);
    agent_log_str("[worker] got task: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    int sum = agent_add(17, 25);
    agent_log_str("[worker] sum=");
    agent_log_int(sum);
    agent_log_str("\n");

    agent_send_msg(planner_id, MSG_RESULT, "sum=42");
    agent_log_str("[worker] done\n");
    agent_exit(0);
}

static void planner_agent(void) {
    struct agent_msg msg;
    agent_log_str("[planner] started\n");

    agent_send_msg(worker_id, MSG_TASK, "compute:17+25");
    agent_log_str("[planner] task dispatched\n");

    agent_recv_msg(&msg);
    agent_log_str("[planner] result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_log_str("[planner] done\n");
    agent_send_msg(1, MSG_RESULT, "pipeline-complete");
    agent_exit(0);
}

void init_agent(void) {
    agent_log_str("[init] AgentOS demo pipeline\n");

    worker_id = agent_spawn(worker_agent, "worker",
                            CAP_LOG | CAP_MATH | CAP_SEND | CAP_RECV);
    planner_id = agent_spawn(planner_agent, "planner",
                             CAP_LOG | CAP_SEND | CAP_RECV);

    agent_log_str("[init] agents spawned, waiting\n");

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str("[init] all agents finished: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_exit(0);
}
