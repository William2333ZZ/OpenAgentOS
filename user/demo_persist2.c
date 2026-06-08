#include "../user/libagent.h"

#define MARKER_PATH "/agent/1/persist2-marker"

static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

void init_persist2_agent(void) {
    char marker[64];

    agent_log_str("[init] v3.5 persist AOS2 migration demo\n");

    if (agent_read_file(MARKER_PATH, marker, sizeof(marker)) < 0) {
        agent_log_str("[init] persist2 marker missing\n");
        agent_exit(1);
    }

    agent_log_str("[init] marker: ");
    agent_log_str(marker);
    agent_log_str("\n");

    if (!str_starts_with(marker, "aos1-seed")) {
        agent_log_str("[init] bad persist2 marker\n");
        agent_exit(1);
    }

    if (agent_sync() < 0) {
        agent_log_str("[init] aos2 sync failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] aos2 sync ok\n");
    agent_log_str("[init] persist2 demo complete\n");
    agent_exit(0);
}
