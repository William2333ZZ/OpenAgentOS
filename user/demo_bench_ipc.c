#include "../user/libagent.h"

#define BENCH_ROUNDS 32
#define PING_AGENT_ID 2

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

void ping_agent_main(void) {
    struct agent_msg msg;
    int i;

    for (i = 0; i < BENCH_ROUNDS; i++) {
        agent_recv_msg(&msg);
        if (agent_send_msg(1, MSG_RESULT, "pong") < 0)
            agent_exit(1);
    }
    agent_exit(0);
}

void init_bench_ipc_agent(void) {
    struct agent_msg msg;
    long t0 = 0;
    long t1 = 0;
    int i;

    agent_log_str("[init] v5.5 ipc bench demo\n");
    agent_yield();

    if (agent_time(&t0) < 0)
        t0 = 0;

    for (i = 0; i < BENCH_ROUNDS; i++) {
        if (agent_send_msg(PING_AGENT_ID, MSG_TASK, "ping") < 0) {
            agent_log_str("[bench-ipc] send failed\n");
            agent_exit(1);
        }
        agent_recv_msg(&msg);
        if (!str_eq(msg.payload, "pong")) {
            agent_log_str("[bench-ipc] bad pong\n");
            agent_exit(1);
        }
    }

    if (agent_time(&t1) < 0)
        t1 = t0;

    agent_log_str("[bench-ipc] rounds=");
    agent_log_int(BENCH_ROUNDS);
    agent_log_str(" ticks=");
    agent_log_int((int)(t1 - t0));
    agent_log_str("\n");
    agent_log_str("[bench-ipc] complete\n");
    agent_exit(0);
}
