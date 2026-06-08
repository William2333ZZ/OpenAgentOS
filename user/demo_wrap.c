#include "../user/libagent.h"

#define STRESS_N       20
#define MARKER_PATH    "/agent/1/wrap-marker"
#define META_PATH      "/agent/1/wrap-meta"

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

static void build_audit_path(int id, char *out) {
    int pos = 0;

    out[pos++] = '/';
    out[pos++] = 'a';
    out[pos++] = 'u';
    out[pos++] = 'd';
    out[pos++] = 'i';
    out[pos++] = 't';
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
    out[pos++] = 't';
    out[pos++] = 'o';
    out[pos++] = 'o';
    out[pos++] = 'l';
    out[pos++] = '.';
    out[pos++] = 'l';
    out[pos++] = 'o';
    out[pos++] = 'g';
    out[pos] = '\0';
}

static int audit_has(const char *audit, const char *needle) {
    int alen = str_len(audit);
    int nlen = str_len(needle);

    if (nlen <= 0 || alen < nlen)
        return 0;
    for (int i = 0; i <= alen - nlen; i++) {
        int j;
        for (j = 0; j < nlen; j++) {
            if (audit[i + j] != needle[j])
                break;
        }
        if (j == nlen)
            return 1;
    }
    return 0;
}

static void boot2_worker(void) {
    char path[64];
    char audit_path[64];
    char buf[64];
    char audit[256];
    int id = agent_self();

    agent_log_str("[wrap] boot2 worker id=");
    agent_log_int(id);
    agent_log_str("\n");

    build_agent_path(id, "stress-19", path);
    if (agent_read_file(path, buf, sizeof(buf)) < 0) {
        agent_log_str("[wrap] stress-19 missing after persist\n");
        agent_exit(1);
    }
    if (buf[0] != 'o' || buf[1] != 'k') {
        agent_log_str("[wrap] stress-19 corrupt\n");
        agent_exit(1);
    }
    agent_log_str("[wrap] stress file restored ok\n");

    build_audit_path(id, audit_path);
    if (agent_read_file(audit_path, audit, sizeof(audit)) < 0) {
        agent_log_str("[wrap] audit missing after persist\n");
        agent_exit(1);
    }
    agent_log_str("[wrap] restored audit: ");
    agent_log_str(audit);
    agent_log_str("\n");

    if (!audit_has(audit, "tool=10 rc=-1")) {
        agent_log_str("[wrap] restored audit missing EPERM\n");
        agent_exit(1);
    }
    agent_log_str("[wrap] audit persist ok\n");
    agent_send_msg(1, MSG_RESULT, "boot2-ok");
    agent_exit(0);
}

static int parse_worker_id(const char *meta) {
    int id = 0;
    int i = 0;

    while (meta[i] >= '0' && meta[i] <= '9') {
        id = id * 10 + (meta[i] - '0');
        i++;
    }
    return id;
}

static void boot1_worker(void) {
    char path[64];
    char audit_path[64];
    char audit[256];
    int id = agent_self();
    int i;

    agent_log_str("[wrap] boot1 worker id=");
    agent_log_int(id);
    agent_log_str("\n");

    if (agent_gpio(1, 1) != EPERM) {
        agent_log_str("[wrap] gpio should EPERM\n");
        agent_exit(1);
    }

    for (i = 0; i < STRESS_N; i++) {
        char name[16];
        char data[8];

        name[0] = 's';
        name[1] = 't';
        name[2] = 'r';
        name[3] = 'e';
        name[4] = 's';
        name[5] = 's';
        name[6] = '-';
        int_to_str(i, name + 7);

        build_agent_path(id, name, path);
        data[0] = 'o';
        data[1] = 'k';
        data[2] = '\0';
        if (agent_write_file(path, data, 2) < 0) {
            agent_log_str("[wrap] stress write failed at ");
            agent_log_int(i);
            agent_log_str("\n");
            agent_exit(1);
        }
    }
    agent_log_str("[wrap] ramfs stress ");
    agent_log_int(STRESS_N);
    agent_log_str(" files ok\n");

    build_audit_path(id, audit_path);
    if (agent_read_file(audit_path, audit, sizeof(audit)) < 0) {
        agent_log_str("[wrap] read audit failed\n");
        agent_exit(1);
    }
    agent_log_str("[wrap] audit: ");
    agent_log_str(audit);
    agent_log_str("\n");

    if (!audit_has(audit, "tool=10 rc=-1")) {
        agent_log_str("[wrap] audit missing EPERM gpio\n");
        agent_exit(1);
    }

    agent_send_msg(1, MSG_RESULT, "boot1-ok");
    agent_exit(0);
}

static void boot2_run(int expect_id) {
    struct agent_msg msg;
    int vid;

    vid = agent_spawn(boot2_worker, "verify", CAP_LOG | CAP_FS | CAP_SEND);
    agent_log_str("[init] boot2 spawned verify id=");
    agent_log_int(vid);
    agent_log_str(" expect=");
    agent_log_int(expect_id);
    agent_log_str("\n");

    if (vid != expect_id) {
        agent_log_str("[init] boot2 worker id mismatch\n");
        agent_exit(1);
    }

    agent_recv_msg(&msg);
    if (msg.type != MSG_RESULT || msg.payload[0] != 'b') {
        agent_log_str("[init] boot2 verify failed\n");
        agent_exit(1);
    }
}

void init_wrap_agent(void) {
    struct agent_msg msg;
    char marker[16];
    char meta[16];
    char idbuf[8];
    int worker_id;

    agent_log_str("[init] v2.x wrap: ramfs+audit persist + uaccess\n");

    if (agent_read_file(MARKER_PATH, marker, sizeof(marker)) >= 0 &&
        marker[0] == '1') {
        if (agent_read_file(META_PATH, meta, sizeof(meta)) < 0) {
            agent_log_str("[init] boot2 meta missing\n");
            agent_exit(1);
        }
        worker_id = parse_worker_id(meta);
        if (worker_id <= 0) {
            agent_log_str("[init] boot2 bad worker id\n");
            agent_exit(1);
        }
        boot2_run(worker_id);
        agent_log_str("[init] wrap boot2 complete\n");
        agent_log_str("[init] wrap demo complete\n");
        agent_exit(0);
    }

    worker_id = agent_spawn(boot1_worker, "stress",
                            CAP_LOG | CAP_FS | CAP_SEND);
    agent_log_str("[init] spawned stress worker id=");
    agent_log_int(worker_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    if (msg.type != MSG_RESULT || msg.payload[0] != 'b') {
        agent_log_str("[init] boot1 worker failed\n");
        agent_exit(1);
    }

    if (agent_write_file(MARKER_PATH, "1", 1) < 0) {
        agent_log_str("[init] write marker failed\n");
        agent_exit(1);
    }
    int_to_str(worker_id, idbuf);
    if (agent_write_file(META_PATH, idbuf, str_len(idbuf)) < 0) {
        agent_log_str("[init] write meta failed\n");
        agent_exit(1);
    }

    if (agent_sync() < 0) {
        agent_log_str("[init] sync failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] persist sync ok\n");
    agent_log_str("[init] wrap boot1 complete\n");
    agent_log_str("[init] wrap demo complete\n");
    agent_exit(0);
}
