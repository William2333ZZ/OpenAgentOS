#include "quota.h"
#include "printf.h"
#include "ramfs.h"
#include "agent.h"
#include "../include/agentos.h"

static int quota_ipc_count[MAX_AGENTS];
static int quota_tool_count[MAX_AGENTS];

static int parse_agent_path_id(const char *path, int *out_id) {
    int id = 0;
    int i;

    if (!path || path[0] != '/')
        return -1;
    if (path[1] != 'a' || path[2] != 'g' || path[3] != 'e' || path[4] != 'n' ||
        path[5] != 't' || path[6] != '/')
        return -1;
    i = 7;
    if (path[i] < '0' || path[i] > '9')
        return -1;
    while (path[i] >= '0' && path[i] <= '9') {
        id = id * 10 + (path[i] - '0');
        i++;
    }
    if (path[i] != '/')
        return -1;
    *out_id = id;
    return 0;
}

void quota_init(void) {
    int i;

    for (i = 0; i < MAX_AGENTS; i++) {
        quota_ipc_count[i] = 0;
        quota_tool_count[i] = 0;
    }
    kprintf("[quota] ipc=%d tool=%d fs_files=%d per agent\n",
            QUOTA_IPC_MAX, QUOTA_TOOL_MAX, QUOTA_FS_FILES);
}

int quota_ipc_used(struct agent *a) {
    if (!a || a->id < 0 || a->id >= MAX_AGENTS)
        return 0;
    return quota_ipc_count[a->id];
}

int quota_ipc_max(struct agent *a) {
    (void)a;
    return QUOTA_IPC_MAX;
}

int quota_ipc_allow(struct agent *a) {
    if (!a)
        return EINVAL;
    if (quota_ipc_used(a) >= quota_ipc_max(a)) {
        kprintf("[quota] ipc exceeded agent=%d used=%d max=%d\n",
                a->id, quota_ipc_used(a), quota_ipc_max(a));
        return ENOSPC;
    }
    return 0;
}

void quota_ipc_charge(struct agent *a) {
    if (!a || a->id < 0 || a->id >= MAX_AGENTS)
        return;
    quota_ipc_count[a->id]++;
}

int quota_tool_used(struct agent *a) {
    if (!a || a->id < 0 || a->id >= MAX_AGENTS)
        return 0;
    return quota_tool_count[a->id];
}

int quota_tool_max(struct agent *a) {
    (void)a;
    return QUOTA_TOOL_MAX;
}

int quota_tool_allow(struct agent *a) {
    if (!a)
        return EINVAL;
    if (quota_tool_used(a) >= quota_tool_max(a)) {
        kprintf("[quota] tool exceeded agent=%d used=%d max=%d\n",
                a->id, quota_tool_used(a), quota_tool_max(a));
        return ENOSPC;
    }
    return 0;
}

void quota_tool_charge(struct agent *a) {
    if (!a || a->id < 0 || a->id >= MAX_AGENTS)
        return;
    quota_tool_count[a->id]++;
}

int quota_fs_used(struct agent *a) {
    if (!a)
        return 0;
    return ramfs_count_agent_files(a->id);
}

int quota_fs_max(struct agent *a) {
    (void)a;
    return QUOTA_FS_FILES;
}

int quota_fs_allow_new(struct agent *a, const char *path) {
    int id;

    if (!a || !path)
        return EINVAL;
    if (parse_agent_path_id(path, &id) != 0)
        return 0;
    if (id != a->id)
        return 0;
    if (ramfs_path_exists(path))
        return 0;
    if (quota_fs_used(a) >= quota_fs_max(a)) {
        kprintf("[quota] fs exceeded agent=%d used=%d max=%d path=%s\n",
                a->id, quota_fs_used(a), quota_fs_max(a), path);
        return ENOSPC;
    }
    return 0;
}

int quota_status(struct agent *a) {
    if (!a)
        return EINVAL;
    kprintf("[quota] agent=%d ipc=%d/%d tool=%d/%d fs=%d/%d\n",
            a->id,
            quota_ipc_used(a), quota_ipc_max(a),
            quota_tool_used(a), quota_tool_max(a),
            quota_fs_used(a), quota_fs_max(a));
    return quota_ipc_used(a);
}

int quota_burn_ipc(struct agent *a, int count) {
    int i;
    int rc;

    if (!a || count <= 0)
        return EINVAL;
    for (i = 0; i < count; i++) {
        rc = agent_send_kernel(a, DISPLAY_AGENT_ID, MSG_RESULT, "quota-burn");
        if (rc != 0)
            return rc;
    }
    return 0;
}

int quota_probe_ipc(struct agent *a) {
    int rc;

    if (!a)
        return EINVAL;
    rc = quota_ipc_allow(a);
    kprintf("[quota] probe agent=%d ipc rc=%d\n", a->id, rc);
    return rc;
}

int quota_notify(struct agent *a, const char *text) {
    if (!a || !text)
        return EINVAL;
    return agent_send_kernel_nocheck(a, 1, MSG_RESULT, text);
}
