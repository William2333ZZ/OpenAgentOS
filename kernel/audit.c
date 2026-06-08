#include "audit.h"
#include "ramfs.h"
#include "printf.h"
#include "../include/agentos.h"

static int parse_audit_agent_id(const char *path) {
    int id = 0;
    int i;

    if (path[1] != 'a' || path[2] != 'u' || path[3] != 'd' || path[4] != 'i' ||
        path[5] != 't' || path[6] != '/')
        return -1;
    if (path[7] != 'a' || path[8] != 'g' || path[9] != 'e' || path[10] != 'n' ||
        path[11] != 't' || path[12] != '/')
        return -1;
    i = 13;
    if (path[i] < '0' || path[i] > '9')
        return -1;
    while (path[i] >= '0' && path[i] <= '9') {
        id = id * 10 + (path[i] - '0');
        i++;
    }
    if (path[i] != '/')
        return -1;
    if (path[i + 1] != 't' || path[i + 2] != 'o' || path[i + 3] != 'o' ||
        path[i + 4] != 'l' || path[i + 5] != '.' || path[i + 6] != 'l' ||
        path[i + 7] != 'o' || path[i + 8] != 'g' || path[i + 9] != '\0')
        return -1;
    return id;
}

void audit_path_for(int agent_id, char *path) {
    int pos = 0;
    int digits[8];
    int nd = 0;
    int n = agent_id;

    path[pos++] = '/';
    path[pos++] = 'a';
    path[pos++] = 'u';
    path[pos++] = 'd';
    path[pos++] = 'i';
    path[pos++] = 't';
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
    path[pos++] = 't';
    path[pos++] = 'o';
    path[pos++] = 'o';
    path[pos++] = 'l';
    path[pos++] = '.';
    path[pos++] = 'l';
    path[pos++] = 'o';
    path[pos++] = 'g';
    path[pos] = '\0';
}

void audit_init(void) {
    kprintf("[audit] tool.log quota=%d bytes per agent\n", AUDIT_QUOTA_BYTES);
}

int audit_usage(struct agent *a) {
    char path[RAMFS_PATH_MAX];
    char buf[4];
    int n;
    int off;

    if (!a)
        return 0;
    audit_path_for(a->id, path);
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

int audit_quota(struct agent *a) {
    (void)a;
    return AUDIT_QUOTA_BYTES;
}

int audit_allow_append(struct agent *a, int add_bytes) {
    int used;
    int quota;

    if (!a || add_bytes < 0)
        return EINVAL;
    used = audit_usage(a);
    quota = audit_quota(a);
    if (used + add_bytes > quota) {
        kprintf("[audit] quota exceeded agent=%d used=%d add=%d quota=%d\n",
                a->id, used, add_bytes, quota);
        return ENOSPC;
    }
    return 0;
}

int audit_path_ok(struct agent *a, const char *path) {
    int id;

    if (!a || !path)
        return 0;
    id = parse_audit_agent_id(path);
    if (id < 0)
        return 0;
    return id == a->id;
}

int audit_probe_cross_read(struct agent *a, int target_id) {
    char path[RAMFS_PATH_MAX];
    char buf[16];
    int rc;

    if (!a)
        return EINVAL;
    if (target_id == a->id)
        return 0;

    audit_path_for(target_id, path);
    if (!audit_path_ok(a, path))
        return EPERM;

    rc = ramfs_read(path, 0, buf, (int)sizeof(buf) - 1);
    if (rc < 0)
        return rc;
    return 0;
}

int audit_write(struct agent *a, const char *data, int len) {
    char path[RAMFS_PATH_MAX];
    char buf[RAMFS_FILE_SIZE];
    int used;
    int pos;
    int i;
    int rc;

    if (!a || !data || len <= 0)
        return EINVAL;
    rc = audit_allow_append(a, len);
    if (rc != 0)
        return rc;
    audit_path_for(a->id, path);
    used = audit_usage(a);
    if (used <= 0)
        return ramfs_put(path, data, len);

    if (used + len >= RAMFS_FILE_SIZE)
        return ENOSPC;

    pos = ramfs_read(path, 0, buf, used);
    if (pos < 0)
        return pos;
    for (i = 0; i < len; i++)
        buf[pos++] = data[i];
    return ramfs_put(path, buf, pos);
}

#define AUDIT_COMPACT_MAX_LINES 256

int audit_compact(struct agent *a, int keep_lines) {
    char path[RAMFS_PATH_MAX];
    static char buf[RAMFS_FILE_SIZE];
    static char out[RAMFS_FILE_SIZE];
    static int line_starts[AUDIT_COMPACT_MAX_LINES];
    int n;
    int line_count = 0;
    int i;
    int start;
    int pos = 0;

    if (!a)
        return EINVAL;
    if (keep_lines < 1)
        keep_lines = 1;
    if (keep_lines > 32)
        keep_lines = 32;

    audit_path_for(a->id, path);
    n = ramfs_read(path, 0, buf, (int)sizeof(buf));
    if (n <= 0)
        return 0;

    line_starts[0] = 0;
    line_count = 1;
    for (i = 0; i < n; i++) {
        if (buf[i] == '\n' && i + 1 < n) {
            if (line_count < AUDIT_COMPACT_MAX_LINES - 1)
                line_starts[line_count++] = i + 1;
        }
    }

    if (line_count <= keep_lines) {
        kprintf("[audit] compact agent=%d keep=%d lines=%d (no-op)\n",
                a->id, keep_lines, line_count);
        return audit_usage(a);
    }

    start = line_starts[line_count - keep_lines];
    out[pos++] = 'c';
    out[pos++] = 'o';
    out[pos++] = 'm';
    out[pos++] = 'p';
    out[pos++] = 'a';
    out[pos++] = 'c';
    out[pos++] = 't';
    out[pos++] = ':';
    out[pos++] = ' ';
    if (keep_lines >= 10)
        out[pos++] = '0' + (keep_lines / 10);
    out[pos++] = '0' + (keep_lines % 10);
    out[pos++] = '\n';

    for (i = start; i < n && pos < (int)sizeof(out) - 1; i++)
        out[pos++] = buf[i];
    out[pos] = '\0';

    ramfs_put(path, out, pos);
    kprintf("[audit] compact agent=%d keep=%d used=%d\n", a->id, keep_lines, pos);
    return pos;
}

int audit_tail(struct agent *a, int max_bytes) {
    char path[RAMFS_PATH_MAX];
    char buf[AUDIT_TAIL_MAX + 1];
    int used;
    int start;
    int n;

    if (!a)
        return EINVAL;
    if (max_bytes <= 0 || max_bytes > AUDIT_TAIL_MAX)
        max_bytes = AUDIT_TAIL_MAX;

    audit_path_for(a->id, path);
    used = audit_usage(a);
    if (used <= 0) {
        kprintf("[audit] tail agent=%d (empty)\n", a->id);
        return 0;
    }

    start = used - max_bytes;
    if (start < 0)
        start = 0;
    n = ramfs_read(path, (unsigned long)start, buf, max_bytes);
    if (n <= 0) {
        kprintf("[audit] tail agent=%d read failed\n", a->id);
        return n < 0 ? n : EIO;
    }
    buf[n] = '\0';
    kprintf("[audit] tail agent=%d: %s", a->id, buf);
    return n;
}
