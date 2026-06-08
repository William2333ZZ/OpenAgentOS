#ifndef QUOTA_H
#define QUOTA_H

#include "agent.h"

#define QUOTA_IPC_MAX   16
#define QUOTA_TOOL_MAX  128
#define QUOTA_FS_FILES  32

void quota_init(void);
int quota_ipc_used(struct agent *a);
int quota_ipc_max(struct agent *a);
int quota_ipc_allow(struct agent *a);
void quota_ipc_charge(struct agent *a);
int quota_tool_used(struct agent *a);
int quota_tool_max(struct agent *a);
int quota_tool_allow(struct agent *a);
void quota_tool_charge(struct agent *a);
int quota_fs_used(struct agent *a);
int quota_fs_max(struct agent *a);
int quota_fs_allow_new(struct agent *a, const char *path);
int quota_status(struct agent *a);
int quota_burn_ipc(struct agent *a, int count);
int quota_probe_ipc(struct agent *a);
int quota_notify(struct agent *a, const char *text);

#endif
