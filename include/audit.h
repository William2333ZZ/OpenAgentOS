#ifndef AUDIT_H
#define AUDIT_H

#include "agent.h"

#define AUDIT_QUOTA_BYTES 512
#define AUDIT_TAIL_MAX    128

void audit_init(void);
void audit_path_for(int agent_id, char *path);
int audit_usage(struct agent *a);
int audit_quota(struct agent *a);
int audit_allow_append(struct agent *a, int add_bytes);
int audit_path_ok(struct agent *a, const char *path);
int audit_probe_cross_read(struct agent *a, int target_id);
int audit_write(struct agent *a, const char *data, int len);
int audit_compact(struct agent *a, int keep_lines);
int audit_tail(struct agent *a, int max_bytes);

#endif
