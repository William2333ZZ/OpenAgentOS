#include "namespace.h"
#include "ramfs.h"
#include "printf.h"
#include "../include/agentos.h"

static int parse_namespace_agent_id(const char *path) {
    int id = 0;
    int i;

    if (path[1] != 'n' || path[2] != 's' || path[3] != '/')
        return -1;
    if (path[4] != 'a' || path[5] != 'g' || path[6] != 'e' || path[7] != 'n' ||
        path[8] != 't' || path[9] != '/')
        return -1;
    i = 10;
    if (path[i] < '0' || path[i] > '9')
        return -1;
    while (path[i] >= '0' && path[i] <= '9') {
        id = id * 10 + (path[i] - '0');
        i++;
    }
    if (path[i] != '/')
        return -1;
    if (path[i + 1] != 'h' || path[i + 2] != 'o' || path[i + 3] != 'm' ||
        path[i + 4] != 'e' || path[i + 5] != '/')
        return -1;
    return id;
}

int namespace_is_path(const char *path) {
    if (!path || path[0] != '/')
        return 0;
    return path[1] == 'n' && path[2] == 's' && path[3] == '/';
}

void namespace_root_for(int agent_id, char *path) {
    int pos = 0;
    int digits[8];
    int nd = 0;
    int n = agent_id;

    path[pos++] = '/';
    path[pos++] = 'n';
    path[pos++] = 's';
    path[pos++] = '/';
    path[pos++] = 'a';
    path[pos++] = 'g';
    path[pos++] = 'e';
    path[pos++] = 'n';
    path[pos++] = 't';
    path[pos++] = '/';

    if (n == 0) {
        path[pos++] = '0';
    } else {
        while (n > 0) {
            digits[nd++] = n % 10;
            n /= 10;
        }
        while (nd > 0)
            path[pos++] = '0' + digits[--nd];
    }

    path[pos++] = '/';
    path[pos++] = 'h';
    path[pos++] = 'o';
    path[pos++] = 'm';
    path[pos++] = 'e';
    path[pos] = '\0';
}

void namespace_file_for(int agent_id, const char *name, char *path) {
    int pos = 0;
    int i;

    namespace_root_for(agent_id, path);
    while (path[pos])
        pos++;
    path[pos++] = '/';
    for (i = 0; name[i] && pos < RAMFS_PATH_MAX - 1; i++)
        path[pos++] = name[i];
    path[pos] = '\0';
}

void namespace_init(void) {
    kprintf("[namespace] mount view /ns/agent/<id>/home quota=%d\n",
            NAMESPACE_HOME_QUOTA);
}

int namespace_home_usage(struct agent *a) {
    char path[RAMFS_PATH_MAX];
    char buf[4];
    int n;
    int off;

    if (!a)
        return 0;
    namespace_file_for(a->id, NAMESPACE_SECRET, path);
    n = ramfs_read(path, 0, buf, 1);
    if (n < 0)
        return 0;
    for (off = 1; off < RAMFS_FILE_SIZE; off++) {
        n = ramfs_read(path, (unsigned long)off, buf, 1);
        if (n <= 0)
            return off;
    }
    return RAMFS_FILE_SIZE;
}

int namespace_home_quota(struct agent *a) {
    (void)a;
    return NAMESPACE_HOME_QUOTA;
}

int namespace_path_ok(struct agent *a, const char *path) {
    int id;

    if (!a || !path)
        return 0;
    if (!namespace_is_path(path))
        return 0;
    id = parse_namespace_agent_id(path);
    if (id < 0)
        return 0;
    return id == a->id;
}

int namespace_write_secret(struct agent *a, const char *data, int len) {
    char path[RAMFS_PATH_MAX];
    int used;
    int quota;

    if (!a || !data || len <= 0)
        return EINVAL;
    used = namespace_home_usage(a);
    quota = namespace_home_quota(a);
    if (used + len > quota) {
        kprintf("[namespace] quota exceeded agent=%d used=%d add=%d quota=%d\n",
                a->id, used, len, quota);
        return ENOSPC;
    }
    namespace_file_for(a->id, NAMESPACE_SECRET, path);
    kprintf("[namespace] agent=%d write secret len=%d\n", a->id, len);
    return ramfs_put(path, data, len);
}

int namespace_probe_cross_read(struct agent *a, int target_id) {
    char path[RAMFS_PATH_MAX];
    char buf[32];
    int rc;

    if (!a)
        return EINVAL;
    if (target_id == a->id)
        return 0;

    namespace_file_for(target_id, NAMESPACE_SECRET, path);
    if (!namespace_path_ok(a, path))
        return EPERM;

    rc = ramfs_read(path, 0, buf, (int)sizeof(buf) - 1);
    if (rc < 0)
        return rc;
    return 0;
}

int namespace_status(struct agent *a) {
    char root[RAMFS_PATH_MAX];
    int used;
    int quota;

    if (!a)
        return EINVAL;
    namespace_root_for(a->id, root);
    used = namespace_home_usage(a);
    quota = namespace_home_quota(a);
    kprintf("[namespace] agent=%d root=%s used=%d quota=%d\n",
            a->id, root, used, quota);
    return used;
}
