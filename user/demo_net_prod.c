#include "../user/libagent.h"

/* QEMU user-net: host is 10.0.2.2 from guest */
#define NET_TEST_URL       "http://10.0.2.2:8080/agentos"
#define NET_DEGRADE_URL    "http://10.0.2.2:8080/reject-degrade"

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

void init_net_prod_agent(void) {
    char body[HTTP_MAX_BODY];
    char scratch[HTTP_MAX_BODY];
    int n;
    int i;

    agent_log_str("[init] v4.3 production network demo\n");

    n = agent_svc_http("http://stub/reject", scratch, sizeof(scratch), "offline");
    if (n >= 0) {
        agent_log_str("[init] offline should have failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] network offline rejected\n");

    n = agent_svc_http(NET_TEST_URL, body, sizeof(body), "native");
    if (n < 0) {
        agent_log_str("[init] native http failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] native body: ");
    agent_log_str(body);
    agent_log_str("\n");
    if (!str_starts_with(body, "200:agentos-net-stub")) {
        agent_log_str("[init] unexpected native body\n");
        agent_exit(1);
    }
    agent_log_str("[init] network native ok\n");

    n = agent_svc_http(NET_DEGRADE_URL, body, sizeof(body), "prod");
    if (n < 0) {
        agent_log_str("[init] prod cache fallback failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] degrade body: ");
    agent_log_str(body);
    agent_log_str("\n");
    if (!str_eq(body, "cache:200:offline-ok")) {
        agent_log_str("[init] unexpected cache fallback body\n");
        agent_exit(1);
    }
    agent_log_str("[init] network cache fallback ok\n");

    for (i = 0; i < 3; i++) {
        n = agent_svc_http(NET_TEST_URL, scratch, sizeof(scratch), "prod");
        if (n < 0) {
            agent_log_str("[init] prod quota fill failed\n");
            agent_exit(1);
        }
    }

    n = agent_svc_http(NET_TEST_URL, scratch, sizeof(scratch), "prod");
    if (n >= 0) {
        agent_log_str("[init] quota should have failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] network quota exceeded\n");

    agent_send_msg(NET_AGENT_ID, MSG_PIPELINE_DONE, "shutdown");
    agent_log_str("[init] net prod demo complete\n");
    agent_exit(0);
}
