#ifndef FLEET_H
#define FLEET_H

#include "agent.h"

#define FLEET_CMD_STATUS 0
#define FLEET_CMD_PUSH   1
#define FLEET_CMD_PROBE  2
#define FLEET_CMD_INGEST 3

#define FLEET_COLLECTOR_PATH "/sys/fleet/collector"

void fleet_init(void);
int fleet_status(struct agent *a);
int fleet_push(struct agent *a);
int fleet_probe(struct agent *a, const char *url);
int fleet_ingest(struct agent *a, const char *url);

#endif
