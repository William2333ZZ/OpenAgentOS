#include "policy.h"
#include "printf.h"
#include "ramfs.h"
#include "../include/agentos.h"

#define POLICY_ACTIVE "/sys/policy/active"

static int deny_tools[POLICY_DENY_MAX];
static int deny_count;

static char net_allow_host[POLICY_NET_ALLOW_MAX][48];
static uint16_t net_allow_port[POLICY_NET_ALLOW_MAX];
static int net_allow_count;
static int net_restrict;

static int host_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

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
    net_allow_count = 0;
    net_restrict = 0;
    if (!ramfs_path_exists(POLICY_ACTIVE))
        ramfs_put(POLICY_ACTIVE, seed_policy, (int)sizeof(seed_policy) - 1);
    kprintf("[policy] ready path=%s deny_max=%d net_allow_max=%d\n", POLICY_ACTIVE,
            POLICY_DENY_MAX, POLICY_NET_ALLOW_MAX);
}

int policy_net_set_restrict(struct agent *a, int on) {
    net_restrict = on ? 1 : 0;
    kprintf("[policy] net restrict=%d agent=%d\n", net_restrict, a ? a->id : -1);
    return net_restrict;
}

int policy_net_add_allow(struct agent *a, const char *host, uint16_t port) {
    int i;

    if (!host || !host[0] || port == 0)
        return EINVAL;
    for (i = 0; i < net_allow_count; i++) {
        if (net_allow_port[i] == port && host_eq(net_allow_host[i], host))
            return 0;
    }
    if (net_allow_count >= POLICY_NET_ALLOW_MAX)
        return ENOSPC;
    i = net_allow_count++;
    {
        int j = 0;
        while (host[j] && j < 47) {
            net_allow_host[i][j] = host[j];
            j++;
        }
        net_allow_host[i][j] = '\0';
    }
    net_allow_port[i] = port;
    kprintf("[policy] net allow %s:%u agent=%d\n", net_allow_host[i], (unsigned)port,
            a ? a->id : -1);
    return 0;
}

int policy_net_status(struct agent *a) {
    int i;

    kprintf("[policy] net restrict=%d allow_count=%d agent=%d\n", net_restrict,
            net_allow_count, a ? a->id : -1);
    for (i = 0; i < net_allow_count; i++)
        kprintf("[policy] net allow %s:%u\n", net_allow_host[i], (unsigned)net_allow_port[i]);
    return net_allow_count;
}

int policy_net_allow(const char *host, uint16_t port) {
    int i;

    if (!host || !host[0])
        return EINVAL;
    if (!net_restrict || net_allow_count == 0)
        return 0;
    for (i = 0; i < net_allow_count; i++) {
        if (net_allow_port[i] == port && host_eq(net_allow_host[i], host))
            return 0;
    }
    kprintf("[policy] net blocked %s:%u\n", host, (unsigned)port);
    return EPERM;
}

int policy_status(struct agent *a) {
    int i;

    kprintf("[policy] agent=%d deny_count=%d active=%s\n",
            a ? a->id : -1, deny_count, POLICY_ACTIVE);
    for (i = 0; i < deny_count; i++)
        kprintf("[policy] deny tool=%d\n", deny_tools[i]);
    return deny_count;
}

static int policy_parse_net_allow(struct agent *a, const char *line) {
    const char *walk = line;
    char host[48];
    int hp = 0;
    unsigned long port = 0;

    while (*walk == ' ' || *walk == '\t')
        walk++;
    if (walk[0] != 'n' || walk[1] != 'e' || walk[2] != 't')
        return 0;
    walk += 3;
    while (*walk == ' ' || *walk == '\t')
        walk++;
    if (walk[0] == 'r' && walk[1] == 'e' && walk[2] == 's' && walk[3] == 't') {
        walk += 4;
        while (*walk == ' ' || *walk == '\t')
            walk++;
        if (walk[0] == 'r' && walk[1] == 'i' && walk[2] == 'c' && walk[3] == 't')
            return policy_net_set_restrict(a, 1);
        if (walk[0] == 'o' && walk[1] == 'p' && walk[2] == 'e' && walk[3] == 'n')
            return policy_net_set_restrict(a, 0);
        return 0;
    }
    if (walk[0] != 'a' || walk[1] != 'l' || walk[2] != 'l' || walk[3] != 'o' ||
        walk[4] != 'w')
        return 0;
    walk += 5;
    while (*walk == ' ' || *walk == '\t')
        walk++;
    while (*walk && *walk != ':' && *walk != ' ' && hp < 47) {
        host[hp++] = *walk++;
    }
    host[hp] = '\0';
    if (!hp)
        return -1;
    if (*walk == ':') {
        walk++;
        while (*walk >= '0' && *walk <= '9') {
            port = port * 10 + (unsigned long)(*walk - '0');
            if (port > 65535)
                return -1;
            walk++;
        }
    }
    if (port == 0)
        port = 443;
    return policy_net_add_allow(a, host, (uint16_t)port);
}

static int policy_parse_line(struct agent *a, const char *line) {
    const char *walk = line;
    int tool = -1;

    while (*walk == ' ' || *walk == '\t')
        walk++;
    if (walk[0] == 'n' && walk[1] == 'e' && walk[2] == 't')
        return policy_parse_net_allow(a, line);
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
