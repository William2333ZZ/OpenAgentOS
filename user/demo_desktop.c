#include "../user/libagent.h"

#include "worker_elf.inc"

#define WORKER_ELF_PATH "/agent/1/worker.agent"
#define SYS_GPU_PATH    "/sys/gpu"

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

static int sys_gpu_ready(void) {
    char buf[4];

    if (agent_read_file(SYS_GPU_PATH, buf, sizeof(buf)) < 1)
        return 0;
    return buf[0] == '1';
}

static int ensure_worker_agent(void) {
    char probe[8];

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) >= 8) {
        agent_log_str("[desktop] worker.agent ready\n");
        return 0;
    }

    agent_log_str("[desktop] seeding worker.agent (");
    agent_log_int((int)worker_elf_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(WORKER_ELF_PATH, (const char *)worker_elf,
                         (int)worker_elf_len) < 0) {
        agent_log_str("[desktop] write worker.agent failed\n");
        return -1;
    }
    return 0;
}

static int handle_load_worker(void) {
    struct agent_msg msg;
    int wid;

    wid = agent_load(WORKER_ELF_PATH, "worker", CAP_LOG | CAP_SEND | CAP_MATH);
    if (wid < 0) {
        agent_log_str("[desktop] agent_load failed\n");
        return -1;
    }

    agent_recv_msg(&msg);
    if (!str_starts_with(msg.payload, "sum=42")) {
        agent_log_str("[desktop] unexpected worker result\n");
        return -1;
    }
    agent_log_str("[desktop] load ok\n");
    return 0;
}

void init_desktop_agent(void) {
    struct agent_msg msg;

    agent_log_str("[init] v5.4 desktop shell demo\n");
    if (ensure_worker_agent() < 0)
        agent_exit(1);

    if (!sys_gpu_ready()) {
        agent_log_str("[init] no GPU — use Console REPL (make run-console)\n");
        agent_log_str("[init] desktop console fallback\n");
        agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
        agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
        agent_send_msg(SHELL_AGENT_ID, MSG_PIPELINE_DONE, "done");
        agent_exit(0);
    }

    agent_log_str("[init] waiting for shell ...\n");

    for (;;) {
        int mtype = agent_recv_msg(&msg);
        if (mtype == (int)MSG_SHELL_REQ) {
            if (handle_load_worker() < 0)
                agent_send_msg(SHELL_AGENT_ID, MSG_SHELL_RSP, "load failed");
            else
                agent_send_msg(SHELL_AGENT_ID, MSG_SHELL_RSP, "load ok");
            continue;
        }
        if (mtype == MSG_RESULT && str_eq(msg.payload, "desktop demo complete")) {
            agent_log_str("[init] desktop demo complete\n");
            agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(SHELL_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }
    }
}
