#include "../user/libagent.h"
#include "../include/ota.h"
#include "../include/catalog.h"

#include "worker_ota_v1_elf.inc"
#include "worker_ota_v2_pkg.inc"

#define WORKER_ELF_PATH "/agent/1/worker.agent"
#define STAGED_PKG      "/agent/1/worker-v2.agentpkg"

#define CATALOG_INDEX \
    "worker|2.0.0|/agent/1/worker-v2.agentpkg|/agent/1/worker.agent|stable|1.0.0\n"

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

static int ensure_worker_v1(void) {
    char probe[8];

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) >= 8) {
        agent_log_str("[catalog] worker v1 ready\n");
        return 0;
    }

    agent_log_str("[catalog] seeding worker v1 (");
    agent_log_int((int)worker_ota_v1_elf_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(WORKER_ELF_PATH, (const char *)worker_ota_v1_elf,
                         (int)worker_ota_v1_elf_len) < 0) {
        agent_log_str("[catalog] write worker v1 failed\n");
        return -1;
    }
    return 0;
}

static int ensure_catalog_index(void) {
    char probe[16];

    if (agent_read_file(CATALOG_PATH_INDEX, probe, sizeof(probe)) >= 8)
        return 0;

    agent_log_str("[catalog] seeding catalog index\n");
    if (agent_write_file(CATALOG_PATH_INDEX, CATALOG_INDEX,
                         (int)(sizeof(CATALOG_INDEX) - 1)) < 0)
        return -1;

    if (agent_write_file(CATALOG_PATH_ACTIVE,
                         "worker|1.0.0|/agent/1/worker.agent", 35) < 0)
        return -1;
    return 0;
}

static int ensure_staged_pkg(void) {
    char probe[8];

    if (agent_read_file(STAGED_PKG, probe, sizeof(probe)) >= 8)
        return 0;

    agent_log_str("[catalog] staging worker-v2 pkg (");
    agent_log_int((int)worker_ota_v2_pkg_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(STAGED_PKG, (const char *)worker_ota_v2_pkg,
                         (int)worker_ota_v2_pkg_len) < 0)
        return -1;
    return 0;
}

static void handle_list(void) {
    char probe[8];

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) >= 8) {
        agent_log_str("[pack] list: ");
        agent_log_str(WORKER_ELF_PATH);
        agent_log_str("\n");
        return;
    }
    agent_log_str("[pack] list: (no .agent packages found)\n");
}

static int handle_load(const char *path, const char *name) {
    struct agent_msg msg;
    int wid;

    if (!path[0] || !name[0]) {
        agent_log_str("[pack] load: missing path or name\n");
        return -1;
    }

    wid = agent_load(path, name, CAP_LOG | CAP_SEND | CAP_MATH);
    if (wid < 0) {
        agent_log_str("[pack] agent_load failed err=");
        agent_log_int(wid);
        agent_log_str("\n");
        return -1;
    }
    agent_log_str("[pack] loaded id=");
    agent_log_int(wid);
    agent_log_str(" path=");
    agent_log_str(path);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    agent_log_str("[pack] worker result: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    return 0;
}

static int handle_catalog(int cmd, const char *arg) {
    int rc;

    rc = (int)sys_agent_tool(TOOL_CATALOG, cmd, (long)arg, 0);
    agent_log_str("[catalog] cmd=");
    agent_log_int(cmd);
    agent_log_str(" rc=");
    agent_log_int(rc);
    if (arg[0]) {
        agent_log_str(" arg=");
        agent_log_str(arg);
    }
    agent_log_str("\n");
    return rc < 0 ? -1 : 0;
}

static void handle_console_req(const char *req) {
    const char *path;
    const char *name;
    char load_path[64];
    char load_name[32];
    int i;
    int sp;

    if (str_eq(req, "list")) {
        handle_list();
        return;
    }
    if (str_starts_with(req, "load:")) {
        i = 5;
        sp = -1;
        while (req[i] && i < (int)sizeof(load_path) - 1) {
            if (req[i] == '|') {
                sp = i;
                break;
            }
            load_path[i - 5] = req[i];
            i++;
        }
        if (sp < 0) {
            agent_log_str("[pack] load: bad payload\n");
            return;
        }
        load_path[sp - 5] = '\0';
        path = load_path;
        i = sp + 1;
        sp = 0;
        while (req[i] && sp < (int)sizeof(load_name) - 1)
            load_name[sp++] = req[i++];
        load_name[sp] = '\0';
        name = load_name;
        (void)handle_load(path, name);
        return;
    }
    if (str_eq(req, "catalog-list")) {
        (void)handle_catalog(CATALOG_CMD_LIST, "");
        return;
    }
    if (str_starts_with(req, "catalog-install:")) {
        (void)handle_catalog(CATALOG_CMD_INSTALL, req + 16);
        return;
    }
    if (str_starts_with(req, "catalog-rollback:")) {
        (void)handle_catalog(CATALOG_CMD_ROLLBACK, req + 17);
        return;
    }
    agent_log_str("[catalog] unknown console req: ");
    agent_log_str(req);
    agent_log_str("\n");
}

void init_catalog_agent(void) {
    struct agent_msg msg;

    agent_log_str("[init] v5.6 package catalog demo\n");
    if (ensure_worker_v1() < 0)
        agent_exit(1);
    if (ensure_catalog_index() < 0)
        agent_exit(1);
    if (ensure_staged_pkg() < 0)
        agent_exit(1);

    agent_log_str("[init] waiting for console commands ...\n");

    for (;;) {
        agent_recv_msg(&msg);
        if (msg.type == MSG_CONSOLE_REQ) {
            handle_console_req(msg.payload);
            continue;
        }
        if (msg.type == MSG_RESULT && str_eq(msg.payload, "catalog demo complete")) {
            agent_log_str("[init] catalog demo complete\n");
            agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }
    }
}
