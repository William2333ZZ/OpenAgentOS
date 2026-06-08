#ifndef TENANT_H
#define TENANT_H

#include "agent.h"

#define TENANT_SESSION_QUOTA 640

void tenant_init(void);
int tenant_session_usage(struct agent *a);
int tenant_session_quota(struct agent *a);
int tenant_session_allow(struct agent *a, int add_bytes);
int tenant_session_path_ok(struct agent *a, const char *path);
int tenant_probe_cross_read(struct agent *a, int target_id);

#endif
