#include "../user/libagent.h"

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

static void build_agent_path(int id, const char *name, char *out) {
    int pos = 0;
    out[pos++] = '/';
    out[pos++] = 'a';
    out[pos++] = 'g';
    out[pos++] = 'e';
    out[pos++] = 'n';
    out[pos++] = 't';
    out[pos++] = '/';
    int_to_str(id, out + pos);
    while (out[pos])
        pos++;
    out[pos++] = '/';
    for (int i = 0; name[i]; i++)
        out[pos++] = name[i];
    out[pos] = '\0';
}

static void worker_agent(void) {
    char version[64];
    char note_path[64];
    char note[64];
    int id = agent_self();
    int t0 = agent_time(0);
    int t1;

    agent_log_str("[worker] reading /sys/version\n");
    if (agent_read_file("/sys/version", version, sizeof(version)) < 0) {
        agent_log_str("[worker] read version failed\n");
        agent_exit(1);
    }
    agent_log_str("[worker] version: ");
    agent_log_str(version);
    agent_log_str("\n");

    build_agent_path(id, "note.txt", note_path);
    agent_log_str("[worker] writing ");
    agent_log_str(note_path);
    agent_log_str("\n");
    if (agent_write_file(note_path, "persisted-by-worker", 19) < 0) {
        agent_log_str("[worker] write failed\n");
        agent_exit(1);
    }

    if (agent_read_file(note_path, note, sizeof(note)) < 0) {
        agent_log_str("[worker] readback failed\n");
        agent_exit(1);
    }
    agent_log_str("[worker] readback: ");
    agent_log_str(note);
    agent_log_str("\n");

    t1 = agent_time(0);
    agent_log_str("[worker] time delta=");
    agent_log_int(t1 - t0);
    agent_log_str("\n");

    agent_send_msg(1, MSG_RESULT, note);
    agent_exit(0);
}

void init_tools_agent(void) {
    agent_log_str("[init] AgentOS system tools demo (ramfs + policy)\n");

    int wid = agent_spawn(worker_agent, "worker",
                          CAP_LOG | CAP_FS | CAP_TIME | CAP_SEND);
    agent_log_str("[init] spawned worker id=");
    agent_log_int(wid);
    agent_log_str("\n");

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str("[init] worker persisted: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_exit(0);
}
