#include "mesh.h"

void mesh_init(void) {}

int mesh_status(struct agent *a) {
    (void)a;
    return 0;
}

int mesh_beacon(struct agent *a) {
    (void)a;
    return 0;
}

int mesh_probe(struct agent *a, const char *service) {
    (void)a;
    (void)service;
    return 0;
}
