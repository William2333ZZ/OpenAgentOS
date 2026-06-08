#include "../user/libagent.h"
#include "../include/ota.h"

#include "worker_elf.inc"

#define WORKER_ELF_PATH "/agent/1/worker.agent"

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

static int ensure_worker_agent(void) {
    char probe[8];

    if (agent_read_file(WORKER_ELF_PATH, probe, sizeof(probe)) >= 8) {
        agent_log_str("[pack] worker.agent ready\n");
        return 0;
    }

    agent_log_str("[pack] seeding worker.agent (");
    agent_log_int((int)worker_elf_len);
    agent_log_str(" bytes)\n");
    if (agent_write_file(WORKER_ELF_PATH, (const char *)worker_elf,
                         (int)worker_elf_len) < 0) {
        agent_log_str("[pack] write worker.agent failed\n");
        return -1;
    }
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

    if (!str_starts_with(msg.payload, "sum=42")) {
        agent_log_str("[pack] unexpected worker result\n");
        return -1;
    }
    agent_log_str("[pack] load ok\n");
    return 0;
}

static int handle_ota(int cmd, const char *path) {
    int rc;

    if (!path[0]) {
        agent_log_str("[pack] ota: missing package path\n");
        return -1;
    }
    rc = (int)sys_agent_tool(TOOL_OTA, cmd, (long)path, 0);
    agent_log_str("[pack] ota rc=");
    agent_log_int(rc);
    agent_log_str(" path=");
    agent_log_str(path);
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
    if (str_starts_with(req, "ota-verify:")) {
        (void)handle_ota(OTA_CMD_VERIFY, req + 11);
        return;
    }
    if (str_starts_with(req, "ota-apply:")) {
        (void)handle_ota(OTA_CMD_APPLY, req + 10);
        return;
    }
    agent_log_str("[pack] unknown console req: ");
    agent_log_str(req);
    agent_log_str("\n");
}

void init_console_pack_agent(void) {
    struct agent_msg msg;

    agent_log_str("[init] v5.3 console pack demo\n");
    if (ensure_worker_agent() < 0)
        agent_exit(1);

    agent_log_str("[init] waiting for console commands ...\n");

    for (;;) {
        agent_recv_msg(&msg);
        if (msg.type == MSG_CONSOLE_REQ) {
            handle_console_req(msg.payload);
            continue;
        }
        if (msg.type == MSG_RESULT && str_eq(msg.payload, "console demo pack complete")) {
            agent_log_str("[init] console demo pack complete\n");
            agent_send_msg(DISPLAY_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_send_msg(INPUT_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }
        if (msg.type == MSG_RESULT && str_eq(msg.payload, "box demo complete")) {
            agent_log_str("[init] box demo complete\n");
            agent_send_msg(CONSOLE_AGENT_ID, MSG_PIPELINE_DONE, "done");
            agent_exit(0);
        }
    }
}
