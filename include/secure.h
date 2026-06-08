#ifndef SECURE_H
#define SECURE_H

#include "agent.h"
#include "ota.h"

#define SECURE_CMD_STATUS  0
#define SECURE_CMD_ENFORCE 1
#define SECURE_CMD_PROBE   2

void secure_init(void);
int secure_status(struct agent *a);
int secure_enforce(struct agent *a, int on);
int secure_probe(struct agent *a);
int secure_ota_gate(const struct ota_manifest *manifest);

#endif
