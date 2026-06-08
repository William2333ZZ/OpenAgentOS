#include "../user/libagent.h"

#include "worker_elf.inc"

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

#define WORKER_ELF_PATH "/agent/1/worker.agent"

void init_load_agent(void) {
    struct agent_msg msg;
    char probe[8];
    int wid;

    agent_log_str("[init] v3.3 agent ELF load demo\n");

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) < 8) {
        agent_log_str("[init] writing worker.agent (");
        agent_log_int((int)worker_elf_len);
        agent_log_str(" bytes)\n");
        if (agent_write_file(WORKER_ELF_PATH, (const char *)worker_elf,
                             (int)worker_elf_len) < 0) {
            agent_log_str("[init] write worker.agent failed\n");
            agent_exit(1);
        }
        if (agent_sync() < 0) {
            agent_log_str("[init] sync failed\n");
            agent_exit(1);
        }
        agent_log_str("[init] worker.agent persisted\n");
    } else {
        agent_log_str("[init] worker.agent restored from ramfs\n");
    }

    wid = agent_load(WORKER_ELF_PATH, "worker",
                     CAP_LOG | CAP_SEND | CAP_MATH);
    if (wid < 0) {
        agent_log_str("[init] agent_load failed err=");
        agent_log_int(wid);
        agent_log_str("\n");
        agent_exit(1);
    }
    agent_log_str("[init] loaded worker id=");
    agent_log_int(wid);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[init] worker result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (!str_starts_with(msg.payload, "sum=42")) {
        agent_log_str("[init] bad worker result\n");
        agent_exit(1);
    }
    agent_log_str("[init] load demo complete\n");
    agent_exit(0);
}
