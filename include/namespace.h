#ifndef NAMESPACE_H
#define NAMESPACE_H

#include "agent.h"

#define NAMESPACE_HOME_QUOTA 256
#define NAMESPACE_SECRET     "secret"

void namespace_init(void);
int namespace_is_path(const char *path);
void namespace_root_for(int agent_id, char *path);
void namespace_file_for(int agent_id, const char *name, char *path);
int namespace_path_ok(struct agent *a, const char *path);
int namespace_home_usage(struct agent *a);
int namespace_home_quota(struct agent *a);
int namespace_write_secret(struct agent *a, const char *data, int len);
int namespace_probe_cross_read(struct agent *a, int target_id);
int namespace_status(struct agent *a);

#endif
