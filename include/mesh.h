#ifndef MESH_H
#define MESH_H

#include "agent.h"

#define MESH_CMD_STATUS 0
#define MESH_CMD_BEACON 1
#define MESH_CMD_PROBE  2

void mesh_init(void);
int mesh_status(struct agent *a);
int mesh_beacon(struct agent *a);
int mesh_probe(struct agent *a, const char *service);

#endif
