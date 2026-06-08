#include "policy.h"

void policy_init(void) {}

int policy_status(struct agent *a) {
    (void)a;
    return 0;
}

int policy_load(struct agent *a, const char *path) {
    (void)a;
    (void)path;
    return 0;
}

int policy_probe_tool(struct agent *a, int tool) {
    (void)a;
    (void)tool;
    return 0;
}

int policy_deny_tool(struct agent *a, int tool) {
    (void)a;
    (void)tool;
    return 0;
}

int policy_allow_tool(struct agent *a, int tool) {
    (void)a;
    (void)tool;
    return 0;
}

int policy_tool_allow(struct agent *a, int tool) {
    (void)a;
    (void)tool;
    return 0;
}
