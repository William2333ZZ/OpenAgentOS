#ifndef REMOTE_H
#define REMOTE_H

#include "agent.h"

#define REMOTE_CMD_STATUS  0
#define REMOTE_CMD_ENABLE  1
#define REMOTE_CMD_DISABLE 2
#define REMOTE_CMD_PING    3
#define REMOTE_CMD_CONNECT 4

#define REMOTE_DEFAULT_PORT 5557

void remote_init(void);
int remote_status(struct agent *a);
int remote_enable(struct agent *a);
int remote_disable(struct agent *a);
int remote_ping(struct agent *a);
int remote_connect(struct agent *a);

#endif
