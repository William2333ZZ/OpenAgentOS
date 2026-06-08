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
    int i;
    int alen = str_len(audit);
    int nlen = str_len(needle);

    if (nlen <= 0 || alen < nlen)
        return 0;
    for (i = 0; i <= alen - nlen; i++) {
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

static void probe_worker(void) {
    char audit_path[64];
    char audit[256];
    char httpbuf[64];
    int id = agent_self();
    int rc;

    agent_log_str("[probe] id=");
    agent_log_int(id);
    agent_log_str(" trying gpio/http without caps\n");

    rc = agent_gpio(3, 1);
    if (rc != EPERM) {
        agent_log_str("[probe] gpio should EPERM\n");
        agent_exit(1);
    }

    rc = agent_http("http://example.com", httpbuf, sizeof(httpbuf));
    if (rc != EPERM) {
        agent_log_str("[probe] http should EPERM\n");
        agent_exit(1);
    }

    build_audit_path(id, audit_path);
    if (agent_read_file(audit_path, audit, sizeof(audit)) < 0) {
        agent_log_str("[probe] read audit failed\n");
        agent_exit(1);
    }
    agent_log_str("[probe] audit: ");
    agent_log_str(audit);
    agent_log_str("\n");

    if (!audit_has(audit, "tool=10 rc=-1") || !audit_has(audit, "tool=11 rc=-1")) {
        agent_log_str("[probe] audit missing eperm entries\n");
        agent_exit(1);
    }

    agent_log_str("[probe] audit eperm ok\n");
    agent_send_msg(1, MSG_RESULT, "probe-ok");
    agent_exit(0);
}

static void allowed_worker(void) {
    char audit_path[64];
    char audit[256];
    char httpbuf[64];
    int id = agent_self();
    int rc;

    agent_log_str("[allowed] id=");
    agent_log_int(id);
    agent_log_str(" gpio/http with caps\n");

    rc = agent_gpio(5, 1);
    if (rc != 1) {
        agent_log_str("[allowed] gpio failed\n");
        agent_exit(1);
    }

    rc = agent_http("http://example.com", httpbuf, sizeof(httpbuf));
    if (rc < 0 || httpbuf[0] != 's') {
        agent_log_str("[allowed] http failed\n");
        agent_exit(1);
    }

    agent_log_str("[allowed] stub=");
    agent_log_str(httpbuf);
    agent_log_str("\n");

    build_audit_path(id, audit_path);
    if (agent_read_file(audit_path, audit, sizeof(audit)) < 0) {
        agent_log_str("[allowed] read audit failed\n");
        agent_exit(1);
    }
    agent_log_str("[allowed] audit: ");
    agent_log_str(audit);
    agent_log_str("\n");

    if (!audit_has(audit, "tool=10 rc=1") || !audit_has(audit, "tool=11 rc=")) {
        agent_log_str("[allowed] audit missing allow entries\n");
        agent_exit(1);
    }

    agent_log_str("[allowed] audit allow ok\n");
    agent_send_msg(1, MSG_RESULT, "allow-ok");
    agent_exit(0);
}

void init_tools2_agent(void) {
    struct agent_msg msg;
    int probe_id;
    int allow_id;

    agent_log_str("[init] v2.2 tool audit + gpio/http demo\n");

    probe_id = agent_spawn(probe_worker, "probe",
                           CAP_LOG | CAP_SEND | CAP_FS);
    agent_log_str("[init] spawned probe id=");
    agent_log_int(probe_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    if (msg.type != MSG_RESULT || msg.payload[0] != 'p') {
        agent_log_str("[init] probe failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] probe done\n");

    allow_id = agent_spawn(allowed_worker, "allowed",
                           CAP_LOG | CAP_GPIO | CAP_NET | CAP_SEND | CAP_FS);
    agent_log_str("[init] spawned allowed id=");
    agent_log_int(allow_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    if (msg.type != MSG_RESULT || msg.payload[0] != 'a') {
        agent_log_str("[init] allowed worker failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] allowed done\n");
    agent_log_str("[init] tools2 demo complete\n");
    agent_exit(0);
}
