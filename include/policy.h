#ifndef POLICY_H
#define POLICY_H

#include "agent.h"

#define POLICY_CMD_STATUS 0
#define POLICY_CMD_LOAD   1
#define POLICY_CMD_PROBE  2
#define POLICY_CMD_DENY   3
#define POLICY_CMD_ALLOW  4

#define POLICY_DENY_MAX 16

void policy_init(void);
int policy_status(struct agent *a);
int policy_load(struct agent *a, const char *path);
int policy_probe_tool(struct agent *a, int tool);
int policy_deny_tool(struct agent *a, int tool);
int policy_allow_tool(struct agent *a, int tool);
int policy_tool_allow(struct agent *a, int tool);

#endif
