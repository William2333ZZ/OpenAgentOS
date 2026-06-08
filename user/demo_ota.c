#include "../user/libagent.h"
#include "../include/ota.h"

#include "worker_ota_v1_elf.inc"
#include "worker_ota_v2_pkg.inc"

#define WORKER_ELF_PATH "/agent/1/worker.agent"
#define MARKER_PATH     "/agent/1/ota-marker"
#define STAGED_PKG      OTA_PATH_STAGED

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

static int demo_ota_kernel_gen(void) {
    return (int)sys_agent_tool(TOOL_OTA, OTA_CMD_KERNEL_GEN, 0, 0);
}

static int demo_ota_apply(const char *path) {
    return (int)sys_agent_tool(TOOL_OTA, OTA_CMD_APPLY, (long)path, 0);
}

static int demo_ota_verify(const char *path) {
    return (int)sys_agent_tool(TOOL_OTA, OTA_CMD_VERIFY, (long)path, 0);
}

static int run_loaded_worker(const char *expect_prefix) {
    struct agent_msg msg;
    int wid;

    wid = agent_load(WORKER_ELF_PATH, "worker",
                     CAP_LOG | CAP_SEND | CAP_MATH);
    if (wid < 0) {
        agent_log_str("[init] agent_load failed err=");
        agent_log_int(wid);
        agent_log_str("\n");
        return wid;
    }
    agent_log_str("[init] loaded worker id=");
    agent_log_int(wid);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[init] worker result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");

    if (!str_starts_with(msg.payload, expect_prefix)) {
        agent_log_str("[init] bad worker result\n");
        return -1;
    }
    return 0;
}

static int ensure_v1_worker(void) {
    char probe[8];

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) >= 8)
        return 0;

    agent_log_str("[init] writing worker v1 (");
    agent_log_int((int)worker_ota_v1_elf_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(WORKER_ELF_PATH, (const char *)worker_ota_v1_elf,
                         (int)worker_ota_v1_elf_len) < 0)
        return -1;
    return 0;
}

static int stage_v2_package(void) {
    char probe[8];

    if (agent_read_file(STAGED_PKG, probe, sizeof(probe)) >= 8)
        return 0;

    agent_log_str("[init] staging worker-v2 pkg (");
    agent_log_int((int)worker_ota_v2_pkg_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(STAGED_PKG, (const char *)worker_ota_v2_pkg,
                         (int)worker_ota_v2_pkg_len) < 0)
        return -1;
    return 0;
}

void init_ota_agent(void) {
    char marker[64];
    int gen;

    agent_log_str("[init] v4.2 OTA demo\n");

    gen = demo_ota_kernel_gen();
    agent_log_str("[init] ota kernel gen=");
    agent_log_int(gen);
    agent_log_str("\n");

    if (agent_read_file(MARKER_PATH, marker, sizeof(marker)) < 0) {
        agent_log_str("[init] boot1: seed marker + v1 worker\n");

        if (agent_write_file(MARKER_PATH, "ota-seed-v1", 11) < 0) {
            agent_log_str("[init] marker write failed\n");
            agent_exit(1);
        }
        if (ensure_v1_worker() < 0) {
            agent_log_str("[init] v1 worker seed failed\n");
            agent_exit(1);
        }
        if (run_loaded_worker("sum=42") < 0) {
            agent_exit(1);
        }

        if (stage_v2_package() < 0) {
            agent_log_str("[init] stage v2 pkg failed\n");
            agent_exit(1);
        }
        if (agent_sync() < 0) {
            agent_log_str("[init] boot1 sync failed\n");
            agent_exit(1);
        }
        agent_log_str("[init] ota boot1 complete\n");
        agent_exit(0);
    }

    agent_log_str("[init] marker restored: ");
    agent_log_str(marker);
    agent_log_str("\n");

    if (!str_starts_with(marker, "ota-seed-v1")) {
        agent_log_str("[init] bad ota marker\n");
        agent_exit(1);
    }

    if (gen < 2) {
        agent_log_str("[init] need ota kernel gen>=2 on boot2\n");
        agent_exit(1);
    }

    if (demo_ota_verify(STAGED_PKG) < 0) {
        agent_log_str("[init] staged pkg verify failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] ota verify ok\n");

    if (demo_ota_apply(STAGED_PKG) < 0) {
        agent_log_str("[init] ota apply failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] ota apply ok\n");

    if (agent_sync() < 0) {
        agent_log_str("[init] post-ota sync failed\n");
        agent_exit(1);
    }

    if (run_loaded_worker("ver=2 sum=84") < 0) {
        agent_exit(1);
    }

    if (agent_sync() < 0) {
        agent_log_str("[init] final sync failed\n");
        agent_exit(1);
    }

    agent_log_str("[init] ramfs marker still ok\n");
    agent_log_str("[init] ota demo complete\n");
    agent_exit(0);
}
