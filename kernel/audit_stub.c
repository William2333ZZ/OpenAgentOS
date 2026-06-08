#include "audit.h"

void audit_init(void) {}

void audit_path_for(int agent_id, char *path) {
    (void)agent_id;
    if (path)
        path[0] = '\0';
}

int audit_usage(struct agent *a) {
    (void)a;
    return 0;
}

int audit_quota(struct agent *a) {
    (void)a;
    return AUDIT_QUOTA_BYTES;
}

int audit_allow_append(struct agent *a, int add_bytes) {
    (void)a;
    (void)add_bytes;
    return 0;
}

int audit_path_ok(struct agent *a, const char *path) {
    (void)a;
    (void)path;
    return 0;
}

int audit_probe_cross_read(struct agent *a, int target_id) {
    (void)a;
    (void)target_id;
    return 0;
}

int audit_write(struct agent *a, const char *data, int len) {
    (void)a;
    (void)data;
    (void)len;
    return 0;
}

int audit_compact(struct agent *a, int keep_lines) {
    (void)a;
    (void)keep_lines;
    return 0;
}

int audit_tail(struct agent *a, int max_bytes) {
    (void)a;
    (void)max_bytes;
    return 0;
}
