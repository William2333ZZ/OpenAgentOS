#include "../user/libagent.h"

#define SMP_TOOL_ONLINE 0
#define SMP_TOOL_TICKS  1
#define SMP_TOOL_HART   2

static int smp_tool(int cmd, int arg) {
    return (int)sys_agent_tool(TOOL_SMP, cmd, arg, 0);
}

static void worker_a(void) {
    int h = smp_tool(SMP_TOOL_HART, 0);

    agent_log_str("[worker-a] hart=");
    agent_log_int(h);
    agent_log_str(" start\n");
    for (volatile int i = 0; i < 80000; i++)
        ;
    agent_send_msg(1, MSG_RESULT, "a");
    agent_log_str("[worker-a] done\n");
    agent_exit(0);
}

static void worker_b(void) {
    int h = smp_tool(SMP_TOOL_HART, 0);

    agent_log_str("[worker-b] hart=");
    agent_log_int(h);
    agent_log_str(" start\n");
    for (volatile int i = 0; i < 80000; i++)
        ;
    agent_send_msg(1, MSG_RESULT, "b");
    agent_log_str("[worker-b] done\n");
    agent_exit(0);
}

void init_smp_agent(void) {
    struct agent_msg msg;
    int wa;
    int wb;
    int ha = -1;
    int hb = -1;
    int online;

    agent_log_str("[init] smp demo: boot hart ok\n");

    online = smp_tool(SMP_TOOL_ONLINE, 0);
    agent_log_str("[init] smp online=");
    agent_log_int(online);
    agent_log_str("\n");

    if (online < 2) {
        agent_log_str("[init] need 2 harts\n");
        agent_exit(1);
    }

    wa = agent_spawn(worker_a, "worker-a", CAP_LOG | CAP_SEND);
    wb = agent_spawn(worker_b, "worker-b", CAP_LOG | CAP_SEND);
    agent_log_str("[init] spawned workers ");
    agent_log_int(wa);
    agent_log_str(" ");
    agent_log_int(wb);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    if (msg.payload[0] == 'a')
        ha = wa;
    else if (msg.payload[0] == 'b')
        hb = wb;

    agent_recv_msg(&msg);
    if (msg.payload[0] == 'a')
        ha = wa;
    else if (msg.payload[0] == 'b')
        hb = wb;

    if (ha < 0 || hb < 0) {
        agent_log_str("[init] worker recv failed\n");
        agent_exit(1);
    }

    agent_log_str("[init] concurrent workers ok\n");
    agent_log_str("[init] smp demo complete\n");
    agent_exit(0);
}
