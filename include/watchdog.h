#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "agent.h"

#define WATCHDOG_CMD_STATUS  0
#define WATCHDOG_CMD_FEED    1
#define WATCHDOG_CMD_ENABLE  2
#define WATCHDOG_CMD_DISABLE 3

void watchdog_init(void);
void watchdog_poll(void);
int watchdog_status(struct agent *a);
int watchdog_feed(struct agent *a);
int watchdog_enable(struct agent *a);
int watchdog_disable(struct agent *a);

#endif
