#include "mesh.h"
#include "printf.h"
#include "../include/platform.h"

#define MESH_DEVICE_ID "box-x86-001"

void mesh_init(void) {
    kprintf("[mesh] beacon ready device=%s\n", MESH_DEVICE_ID);
}

int mesh_status(struct agent *a) {
    kprintf("[mesh] agent=%d device=%s services=console,fleet,policy,remote\n",
            a ? a->id : -1, MESH_DEVICE_ID);
    return 0;
}

int mesh_beacon(struct agent *a) {
    kprintf("[mesh] beacon device=%s platform=%s udp=5353 services=console,fleet\n",
            MESH_DEVICE_ID, platform_name());
    (void)a;
    return 0;
}

int mesh_probe(struct agent *a, const char *service) {
    if (!service || !service[0])
        return EINVAL;
    kprintf("[mesh] probe agent=%d service=%s local=1 hop=0\n",
            a ? a->id : -1, service);
    return 0;
}
