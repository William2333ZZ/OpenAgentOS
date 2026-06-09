#ifndef POLICY_H
#define POLICY_H

#include "agent.h"

#include <stdint.h>

#define POLICY_CMD_STATUS     0
#define POLICY_CMD_LOAD       1
#define POLICY_CMD_PROBE      2
#define POLICY_CMD_DENY       3
#define POLICY_CMD_ALLOW      4
#define POLICY_CMD_NET_ALLOW  5
#define POLICY_CMD_NET_STATUS 6
#define POLICY_CMD_NET_RESTRICT 7

#define POLICY_DENY_MAX 16
#define POLICY_NET_ALLOW_MAX 8

void policy_init(void);
int policy_status(struct agent *a);
int policy_load(struct agent *a, const char *path);
int policy_probe_tool(struct agent *a, int tool);
int policy_deny_tool(struct agent *a, int tool);
int policy_allow_tool(struct agent *a, int tool);
int policy_tool_allow(struct agent *a, int tool);
int policy_net_allow(const char *host, uint16_t port);
int policy_net_add_allow(struct agent *a, const char *host, uint16_t port);
int policy_net_set_restrict(struct agent *a, int on);
int policy_net_status(struct agent *a);

#endif
