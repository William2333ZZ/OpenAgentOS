#include "tenant.h"
#include "../include/agentos.h"

void tenant_init(void) {}

int tenant_session_usage(struct agent *a) {
    (void)a;
    return 0;
}

int tenant_session_quota(struct agent *a) {
    (void)a;
    return 0;
}

int tenant_session_allow(struct agent *a, int add_bytes) {
    (void)a;
    (void)add_bytes;
    return 0;
}

int tenant_session_path_ok(struct agent *a, const char *path) {
    (void)a;
    (void)path;
    return 1;
}

int tenant_probe_cross_read(struct agent *a, int target_id) {
    (void)a;
    (void)target_id;
    return ENODEV;
}
