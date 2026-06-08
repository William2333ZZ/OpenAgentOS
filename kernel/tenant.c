#include "tenant.h"
#include "session.h"
#include "ramfs.h"
#include "printf.h"
#include "../include/agentos.h"

static int parse_session_agent_id(const char *path) {
    int id = 0;
    int i;

    if (path[1] != 's' || path[2] != 'e' || path[3] != 's' || path[4] != 's' ||
        path[5] != 'i' || path[6] != 'o' || path[7] != '/')
        return -1;
    if (path[8] != 'a' || path[9] != 'g' || path[10] != 'e' || path[11] != 'n' ||
        path[12] != 't' || path[13] != '/')
        return -1;
    i = 14;
    if (path[i] < '0' || path[i] > '9')
        return -1;
    while (path[i] >= '0' && path[i] <= '9') {
        id = id * 10 + (path[i] - '0');
        i++;
    }
    if (path[i] != '/')
        return -1;
    if (path[i + 1] != 'l' || path[i + 2] != 'o' || path[i + 3] != 'g' ||
        path[i + 4] != '\0')
        return -1;
    return id;
}

void tenant_init(void) {
    kprintf("[tenant] session quota=%d bytes per agent\n", TENANT_SESSION_QUOTA);
}

int tenant_session_usage(struct agent *a) {
    char path[64];
    char buf[4];
    int n;
    int off;

    if (!a)
        return 0;
    path[0] = '/';
    path[1] = 's';
    path[2] = 'e';
    path[3] = 's';
    path[4] = 's';
    path[5] = 'i';
    path[6] = 'o';
    path[7] = 'n';
    path[8] = '/';
    path[9] = 'a';
    path[10] = 'g';
    path[11] = 'e';
    path[12] = 'n';
    path[13] = 't';
    path[14] = '/';
    {
        int id = a->id;
        int pos = 15;
        char digits[8];
        int nd = 0;
        if (id == 0) {
            path[pos++] = '0';
        } else {
            while (id > 0) {
                digits[nd++] = (char)('0' + (id % 10));
                id /= 10;
            }
            while (nd > 0)
                path[pos++] = digits[--nd];
        }
        path[pos++] = '/';
        path[pos++] = 'l';
        path[pos++] = 'o';
        path[pos++] = 'g';
        path[pos] = '\0';
    }

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

int tenant_session_quota(struct agent *a) {
    (void)a;
    return TENANT_SESSION_QUOTA;
}

int tenant_session_allow(struct agent *a, int add_bytes) {
    int used;
    int quota;

    if (!a || add_bytes < 0)
        return EINVAL;
    used = tenant_session_usage(a);
    quota = tenant_session_quota(a);
    if (used + add_bytes > quota) {
        kprintf("[tenant] quota exceeded agent=%d used=%d add=%d quota=%d\n",
                a->id, used, add_bytes, quota);
        return ENOSPC;
    }
    return 0;
}

int tenant_session_path_ok(struct agent *a, const char *path) {
    int id;

    if (!a || !path)
        return 0;
    id = parse_session_agent_id(path);
    if (id < 0)
        return 0;
    return id == a->id;
}

int tenant_probe_cross_read(struct agent *a, int target_id) {
    char path[48];
    char buf[16];
    int pos = 0;
    int id = target_id;
    int nd = 0;
    char digits[8];
    int rc;

    if (!a)
        return EINVAL;
    if (target_id == a->id)
        return 0;

    path[pos++] = '/';
    path[pos++] = 's';
    path[pos++] = 'e';
    path[pos++] = 's';
    path[pos++] = 's';
    path[pos++] = 'i';
    path[pos++] = 'o';
    path[pos++] = 'n';
    path[pos++] = '/';
    path[pos++] = 'a';
    path[pos++] = 'g';
    path[pos++] = 'e';
    path[pos++] = 'n';
    path[pos++] = 't';
    path[pos++] = '/';
    if (id == 0) {
        path[pos++] = '0';
    } else {
        while (id > 0) {
            digits[nd++] = (char)('0' + (id % 10));
            id /= 10;
        }
        while (nd > 0)
            path[pos++] = digits[--nd];
    }
    path[pos++] = '/';
    path[pos++] = 'l';
    path[pos++] = 'o';
    path[pos++] = 'g';
    path[pos] = '\0';

    if (!tenant_session_path_ok(a, path))
        return EPERM;

    rc = ramfs_read(path, 0, buf, (int)sizeof(buf) - 1);
    if (rc < 0)
        return rc;
    return 0;
}
