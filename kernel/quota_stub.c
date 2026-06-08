#include "quota.h"

void quota_init(void) {}

int quota_ipc_used(struct agent *a) {
    (void)a;
    return 0;
}

int quota_ipc_max(struct agent *a) {
    (void)a;
    return QUOTA_IPC_MAX;
}

int quota_ipc_allow(struct agent *a) {
    (void)a;
    return 0;
}

void quota_ipc_charge(struct agent *a) {
    (void)a;
}

int quota_tool_used(struct agent *a) {
    (void)a;
    return 0;
}

int quota_tool_max(struct agent *a) {
    (void)a;
    return QUOTA_TOOL_MAX;
}

int quota_tool_allow(struct agent *a) {
    (void)a;
    return 0;
}

void quota_tool_charge(struct agent *a) {
    (void)a;
}

int quota_fs_used(struct agent *a) {
    (void)a;
    return 0;
}

int quota_fs_max(struct agent *a) {
    (void)a;
    return QUOTA_FS_FILES;
}

int quota_fs_allow_new(struct agent *a, const char *path) {
    (void)a;
    (void)path;
    return 0;
}

int quota_status(struct agent *a) {
    (void)a;
    return 0;
}

int quota_burn_ipc(struct agent *a, int count) {
    (void)a;
    (void)count;
    return 0;
}

int quota_probe_ipc(struct agent *a) {
    (void)a;
    return 0;
}

int quota_notify(struct agent *a, const char *text) {
    (void)a;
    (void)text;
    return 0;
}
