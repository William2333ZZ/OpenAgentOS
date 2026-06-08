#include "libagent.h"
#include "orch_pipeline.h"

static int planner_id;
static int worker_id;
static int llm_worker_id;

static void int_to_str(int v, char *out) {
    int i = 0;
    char tmp[12];
    unsigned int n;
    int pos = 0;

    if (v < 0) {
        out[pos++] = '-';
        n = (unsigned int)(-v);
    } else {
        n = (unsigned int)v;
    }
    if (n == 0) {
        out[pos++] = '0';
        out[pos] = '\0';
        return;
    }
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        out[pos++] = tmp[--i];
    out[pos] = '\0';
}

static void str_cat(char *dst, const char *src, int maxlen) {
    int pos = 0;
    while (dst[pos])
        pos++;
    while (src[0] && pos < maxlen - 1) {
        dst[pos++] = src[0];
        src++;
    }
    dst[pos] = '\0';
}

static void worker_agent(void) {
    struct agent_msg msg;
    char version[64];
    char result[MSG_PAYLOAD_SIZE];

    agent_log_str("[worker] started\n");
    agent_recv_msg(&msg);
    agent_log_str("[worker] task: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    {
        int sum = agent_add(17, 25);
        agent_log_str("[worker] sum=");
        agent_log_int(sum);
        agent_log_str("\n");

        if (agent_svc_read("/sys/version", version, sizeof(version)) < 0) {
            agent_log_str("[worker] svc_read failed\n");
            agent_exit(1);
        }
        agent_log_str("[worker] version: ");
        agent_log_str(version);
        agent_log_str("\n");

        result[0] = '\0';
        str_cat(result, "sum=", sizeof(result));
        int_to_str(sum, result + str_len(result));
        str_cat(result, ";version=", sizeof(result));
        str_cat(result, version, sizeof(result));

        agent_send_msg(planner_id, MSG_RESULT, result);
    }
    agent_log_str("[worker] done\n");
    agent_exit(0);
}

static void llm_worker_agent(void) {
    struct agent_msg msg;
    char prompt[LLM_MAX_PROMPT];
    char response[LLM_MAX_RESPONSE];
    int n;

    agent_log_str("[llm-worker] started\n");
    agent_recv_msg(&msg);
    agent_log_str("[llm-worker] task: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    prompt[0] = '\0';
    str_cat(prompt, "Summarize in one short sentence: ", sizeof(prompt));
    str_cat(prompt, msg.payload, sizeof(prompt));

    agent_log_str("[llm-worker] querying LLM...\n");
    n = agent_llm(prompt, response, sizeof(response));
    if (n < 0) {
        agent_log_str("[llm-worker] query failed\n");
        agent_exit(1);
    }
    agent_log_str("[llm-worker] summary: ");
    agent_log_str(response);
    agent_log_str("\n");

    agent_send_msg(planner_id, MSG_RESULT, response);
    agent_log_str("[llm-worker] done\n");
    agent_exit(0);
}

static void planner_agent(void) {
    struct agent_msg msg;

    planner_id = agent_self();
    agent_log_str("[planner] started id=");
    agent_log_int(planner_id);
    agent_log_str("\n");

    worker_id = agent_spawn(worker_agent, "worker",
                            CAP_LOG | CAP_MATH | CAP_SEND | CAP_RECV);
    llm_worker_id = agent_spawn(llm_worker_agent, "llm-worker",
                                CAP_LOG | CAP_LLM | CAP_SEND | CAP_RECV);
    agent_log_str("[planner] spawned worker=");
    agent_log_int(worker_id);
    agent_log_str(" llm-worker=");
    agent_log_int(llm_worker_id);
    agent_log_str("\n");

    agent_send_msg(worker_id, MSG_TASK, "compute:17+25");
    agent_log_str("[planner] dispatched worker task\n");
    agent_recv_msg(&msg);
    agent_log_str("[planner] worker result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    agent_send_msg(llm_worker_id, MSG_TASK, msg.payload);
    agent_log_str("[planner] dispatched llm task\n");
    agent_recv_msg(&msg);
    agent_log_str("[planner] llm summary: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    agent_send_msg(1, MSG_PIPELINE_DONE, "orchestrator-complete");
    agent_log_str("[planner] orchestrator complete\n");
    agent_exit(0);
}

int orch_run_pipeline(void) {
    struct agent_msg msg;
    int pid;

    pid = agent_spawn(planner_agent, "planner",
                      CAP_LOG | CAP_SPAWN | CAP_SEND | CAP_RECV);
    if (pid < 0)
        return -1;
    agent_log_str("[orch] spawned planner id=");
    agent_log_int(pid);
    agent_log_str("\n");

    for (;;) {
        agent_recv_msg(&msg);
        if (msg.type == MSG_PIPELINE_DONE)
            break;
    }
    agent_log_str("[orch] pipeline finished: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    return 0;
}
