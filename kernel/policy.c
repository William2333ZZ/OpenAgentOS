#include "policy.h"
#include "printf.h"
#include "ramfs.h"
#include "../include/agentos.h"

#define POLICY_ACTIVE "/sys/policy/active"

static int deny_tools[POLICY_DENY_MAX];
static int deny_count;

static int deny_has(int tool) {
    int i;

    for (i = 0; i < deny_count; i++) {
        if (deny_tools[i] == tool)
            return 1;
    }
    return 0;
}

static int policy_exempt(int tool) {
    return tool == TOOL_POLICY || tool == TOOL_FLEET || tool == TOOL_REMOTE ||
           tool == TOOL_MESH || tool == TOOL_QUOTA;
}

void policy_init(void) {
    static const char seed_policy[] = "deny 10\n";

    deny_count = 0;
    if (!ramfs_path_exists(POLICY_ACTIVE))
        ramfs_put(POLICY_ACTIVE, seed_policy, (int)sizeof(seed_policy) - 1);
    kprintf("[policy] ready path=%s deny_max=%d\n", POLICY_ACTIVE, POLICY_DENY_MAX);
}

int policy_status(struct agent *a) {
    int i;

    kprintf("[policy] agent=%d deny_count=%d active=%s\n",
            a ? a->id : -1, deny_count, POLICY_ACTIVE);
    for (i = 0; i < deny_count; i++)
        kprintf("[policy] deny tool=%d\n", deny_tools[i]);
    return deny_count;
}

static int policy_parse_line(struct agent *a, const char *line) {
    const char *walk = line;
    int tool = -1;

    while (*walk == ' ' || *walk == '\t')
        walk++;
    if (walk[0] == 'd' && walk[1] == 'e' && walk[2] == 'n' && walk[3] == 'y') {
        walk += 4;
        while (*walk == ' ' || *walk == '\t')
            walk++;
        if (*walk < '0' || *walk > '9')
            return -1;
        tool = 0;
        while (*walk >= '0' && *walk <= '9') {
            tool = tool * 10 + (*walk - '0');
            walk++;
        }
        return policy_deny_tool(a, tool);
    }
    return 0;
}

int policy_load(struct agent *a, const char *path) {
    char buf[RAMFS_FILE_SIZE];
    int len;
    int i;
    int start;

    if (!path || !path[0])
        path = POLICY_ACTIVE;
    len = ramfs_read(path, 0, buf, (int)sizeof(buf) - 1);
    if (len < 0) {
        kprintf("[policy] load agent=%d path=%s rc=%d\n", a ? a->id : -1, path, len);
        return len;
    }
    buf[len] = '\0';
    deny_count = 0;
    start = 0;
    for (i = 0; i <= len; i++) {
        if (buf[i] == '\n' || buf[i] == '\0') {
            char saved = buf[i];
            buf[i] = '\0';
            if (buf[start])
                (void)policy_parse_line(a, buf + start);
            buf[i] = saved;
            start = i + 1;
        }
    }
    kprintf("[policy] load agent=%d path=%s rules=%d\n", a ? a->id : -1, path, deny_count);
    return deny_count;
}

int policy_probe_tool(struct agent *a, int tool) {
    int rc = policy_tool_allow(a, tool);

    kprintf("[policy] probe agent=%d tool=%d rc=%d\n", a ? a->id : -1, tool, rc);
    return rc;
}

int policy_deny_tool(struct agent *a, int tool) {
    if (tool < 0 || tool > 31)
        return EINVAL;
    if (policy_exempt(tool))
        return EINVAL;
    if (deny_has(tool))
        return 0;
    if (deny_count >= POLICY_DENY_MAX)
        return ENOSPC;
    deny_tools[deny_count++] = tool;
    kprintf("[policy] deny agent=%d tool=%d\n", a ? a->id : -1, tool);
    return 0;
}

int policy_allow_tool(struct agent *a, int tool) {
    int i;
    int j;

    for (i = 0; i < deny_count; i++) {
        if (deny_tools[i] != tool)
            continue;
        for (j = i + 1; j < deny_count; j++)
            deny_tools[j - 1] = deny_tools[j];
        deny_count--;
        kprintf("[policy] allow agent=%d tool=%d\n", a ? a->id : -1, tool);
        return 0;
    }
    (void)a;
    return 0;
}

int policy_tool_allow(struct agent *a, int tool) {
    (void)a;
    if (policy_exempt(tool))
        return 0;
    if (deny_has(tool)) {
        kprintf("[policy] blocked tool=%d\n", tool);
        return EPERM;
    }
    return 0;
}
