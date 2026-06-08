#include "../user/libagent.h"

static int worker_id;
static int planner_id;

static int parse_b_from_steering(const char *payload) {
    const char *p = payload;
    while (*p) {
        if (p[0] == 'b' && p[1] == '=') {
            int v = 0;
            p += 2;
            while (*p >= '0' && *p <= '9') {
                v = v * 10 + (*p - '0');
                p++;
            }
            return v;
        }
        p++;
    }
    return 25;
}

static void worker_agent(void) {
    struct agent_msg msg;
    int a = 17;
    int b = 25;

    agent_log_str("[worker] started\n");
    agent_recv_msg(&msg);
    agent_log_str("[worker] got task: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    agent_log_str("[worker] working");
    for (int step = 0; step < 64; step++) {
        struct agent_msg incoming;
        int rc = agent_try_recv(&incoming);
        if (rc == MSG_STEER) {
            agent_log_str(" steered: ");
            agent_log_str(incoming.payload);
            b = parse_b_from_steering(incoming.payload);
            break;
        }
        if (rc == MSG_FOLLOWUP)
            continue;
        agent_log_str(".");
        agent_yield();
    }
    agent_log_str("\n");

    int sum = agent_add(a, b);
    agent_log_str("[worker] sum=");
    agent_log_int(sum);
    agent_log_str(" phase=");
    agent_log_int(agent_phase());
    agent_log_str("\n");

    agent_send_msg(planner_id, MSG_RESULT, "done");
    agent_log_str("[worker] done\n");
    agent_exit(0);
}

static void planner_agent(void) {
    struct agent_msg msg;

    agent_log_str("[planner] started\n");
    agent_compact_inbox(3);
    agent_log_str("[planner] compacted init followups\n");
    while (agent_try_recv(&msg) >= 0)
        ;

    agent_send_msg(worker_id, MSG_TASK, "compute:17+25");
    agent_log_str("[planner] task dispatched, injecting steer\n");

    for (int i = 0; i < 8; i++)
        agent_yield();
    agent_send_steering(worker_id, "b=30");

    while (1) {
        agent_recv_msg(&msg);
        if (msg.type == MSG_RESULT)
            break;
    }
    agent_log_str("[planner] result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    agent_send_msg(1, MSG_PIPELINE_DONE, "harness-complete");
    agent_log_str("[planner] done\n");
    agent_exit(0);
}

void init_harness_agent(void) {
    agent_log_str("[init] AgentOS harness demo (pi-inspired IPC)\n");

    worker_id = agent_spawn(worker_agent, "worker",
                            CAP_LOG | CAP_MATH | CAP_SEND | CAP_RECV);
    planner_id = agent_spawn(planner_agent, "planner",
                             CAP_LOG | CAP_SEND | CAP_RECV);

    for (int i = 0; i < 10; i++)
        agent_send_followup(planner_id, "warmup");

    agent_log_str("[init] agents spawned, waiting\n");

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str("[init] finished: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_exit(0);
}
