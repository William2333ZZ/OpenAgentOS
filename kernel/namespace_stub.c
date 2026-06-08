#include "namespace.h"

void namespace_init(void) {}

int namespace_is_path(const char *path) {
    (void)path;
    return 0;
}

void namespace_root_for(int agent_id, char *path) {
    (void)agent_id;
    if (path)
        path[0] = '\0';
}

void namespace_file_for(int agent_id, const char *name, char *path) {
    (void)agent_id;
    (void)name;
    if (path)
        path[0] = '\0';
}

int namespace_path_ok(struct agent *a, const char *path) {
    (void)a;
    (void)path;
    return 0;
}

int namespace_home_usage(struct agent *a) {
    (void)a;
    return 0;
}

int namespace_home_quota(struct agent *a) {
    (void)a;
    return NAMESPACE_HOME_QUOTA;
}

int namespace_write_secret(struct agent *a, const char *data, int len) {
    (void)a;
    (void)data;
    (void)len;
    return 0;
}

int namespace_probe_cross_read(struct agent *a, int target_id) {
    (void)a;
    (void)target_id;
    return 0;
}

int namespace_status(struct agent *a) {
    (void)a;
    return 0;
}
