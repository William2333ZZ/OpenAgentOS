#include "catalog.h"
#include "ota.h"
#include "agent.h"
#include "printf.h"
#include "ramfs.h"
#include "uaccess.h"
#include "../include/agentos.h"

struct catalog_entry {
    char name[32];
    char version[16];
    char pkg_path[48];
    char install_path[48];
    char channel[8];
    char min_version[16];
};

static void *memcpy_local(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static int catalog_file_size(const char *path) {
    char tmp[4];
    int n;

    n = ramfs_read(path, 0, tmp, 1);
    if (n < 0)
        return n;
    for (int off = 1; off < RAMFS_FILE_SIZE; off++) {
        n = ramfs_read(path, (unsigned long)off, tmp, 1);
        if (n <= 0)
            return off;
    }
    return RAMFS_FILE_SIZE;
}

static int catalog_read_file(const char *path, char *buf, int cap) {
    int len;

    len = catalog_file_size(path);
    if (len < 0)
        return len;
    if (len >= cap)
        return ENOSPC;
    if (ramfs_read(path, 0, buf, len) != len)
        return EIO;
    buf[len] = '\0';
    return len;
}

static int catalog_copy_file(const char *src, const char *dst) {
    char buf[RAMFS_FILE_SIZE];
    int len;
    int n;

    len = catalog_file_size(src);
    if (len < 0)
        return len;
    if (len == 0)
        return ENOENT;
    if (len > (int)sizeof(buf))
        return ENOSPC;
    n = ramfs_read(src, 0, buf, len);
    if (n != len)
        return EIO;
    if (ramfs_put(dst, buf, len) != 0)
        return ENOSPC;
    return 0;
}

static int parse_version(const char *s, int *maj, int *min, int *patch) {
    int m = 0;
    int n = 0;
    int p = 0;
    int part = 0;
    int i = 0;

    *maj = *min = *patch = 0;
    while (s[i]) {
        if (s[i] >= '0' && s[i] <= '9') {
            if (part == 0)
                m = m * 10 + (s[i] - '0');
            else if (part == 1)
                n = n * 10 + (s[i] - '0');
            else
                p = p * 10 + (s[i] - '0');
        } else if (s[i] == '.') {
            part++;
            if (part > 2)
                return EINVAL;
        } else {
            break;
        }
        i++;
    }
    *maj = m;
    *min = n;
    *patch = p;
    return 0;
}

static int version_ge(const char *a, const char *b) {
    int am, an, ap;
    int bm, bn, bp;

    if (parse_version(a, &am, &an, &ap) != 0)
        return 0;
    if (parse_version(b, &bm, &bn, &bp) != 0)
        return 1;
    if (am != bm)
        return am > bm;
    if (an != bn)
        return an > bn;
    return ap >= bp;
}

static int split_field(char *line, int field, char *out, int outlen) {
    int idx = 0;
    int pos = 0;
    char *walk = line;

    while (*walk && idx < field) {
        if (*walk == '|') {
            idx++;
            if (idx == field)
                break;
        }
        walk++;
    }
    if (idx != field)
        return 0;
    if (field > 0)
        walk++;
    while (*walk && *walk != '|' && pos < outlen - 1)
        out[pos++] = *walk++;
    out[pos] = '\0';
    return pos > 0;
}

static int catalog_find_entry(const char *name, struct catalog_entry *out) {
    char index[RAMFS_FILE_SIZE];
    char entry_name[32];
    int len;
    int i;

    len = catalog_read_file(CATALOG_PATH_INDEX, index, (int)sizeof(index));
    if (len < 0)
        return len;

    for (i = 0; i < len; i++) {
        int j = i;

        while (j < len && index[j] != '\n')
            j++;
        index[j] = '\0';
        if (split_field(index + i, 0, entry_name, (int)sizeof(entry_name)) &&
            str_eq(entry_name, name)) {
            split_field(index + i, 1, out->version, (int)sizeof(out->version));
            split_field(index + i, 2, out->pkg_path, (int)sizeof(out->pkg_path));
            split_field(index + i, 3, out->install_path, (int)sizeof(out->install_path));
            split_field(index + i, 4, out->channel, (int)sizeof(out->channel));
            split_field(index + i, 5, out->min_version, (int)sizeof(out->min_version));
            return 0;
        }
        i = j;
    }
    return ENOENT;
}

static int catalog_active_line(const char *name, char *line, int linelen) {
    char active[RAMFS_FILE_SIZE];
    char entry_name[32];
    int len;
    int i;

    len = catalog_read_file(CATALOG_PATH_ACTIVE, active, (int)sizeof(active));
    if (len < 0)
        return len;

    for (i = 0; i < len; i++) {
        int j = i;

        while (j < len && active[j] != '\n')
            j++;
        active[j] = '\0';
        if (split_field(active + i, 0, entry_name, (int)sizeof(entry_name)) &&
            str_eq(entry_name, name)) {
            int k = 0;
            const char *src = active + i;

            while (*src && k < linelen - 1)
                line[k++] = *src++;
            line[k] = '\0';
            return 0;
        }
        i = j;
    }
    return ENOENT;
}

static int catalog_active_version(const char *name, char *ver, int verlen) {
    char line[128];

    if (catalog_active_line(name, line, (int)sizeof(line)) != 0)
        return ENOENT;
    if (!split_field(line, 1, ver, verlen))
        return ENOENT;
    return 0;
}

static int catalog_active_install(const char *name, char *path, int pathlen) {
    char line[128];

    if (catalog_active_line(name, line, (int)sizeof(line)) != 0)
        return ENOENT;
    if (!split_field(line, 2, path, pathlen))
        return ENOENT;
    return 0;
}

static int catalog_set_active(const char *name, const char *version,
                              const char *install_path) {
    char line[128];
    int pos = 0;
    int i;

    for (i = 0; name[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = name[i];
    line[pos++] = '|';
    for (i = 0; version[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = version[i];
    line[pos++] = '|';
    for (i = 0; install_path[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = install_path[i];
    line[pos] = '\0';
    return ramfs_put(CATALOG_PATH_ACTIVE, line, pos) ? ENOSPC : 0;
}

static int catalog_set_rollback(const char *name, const char *version,
                                const char *backup_path) {
    char line[128];
    int pos = 0;
    int i;

    for (i = 0; name[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = name[i];
    line[pos++] = '|';
    for (i = 0; version[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = version[i];
    line[pos++] = '|';
    for (i = 0; backup_path[i] && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = backup_path[i];
    line[pos] = '\0';
    return ramfs_put(CATALOG_PATH_ROLLBACK, line, pos) ? ENOSPC : 0;
}

static int catalog_read_rollback(const char *name, char *version, int verlen,
                                 char *backup, int backlen) {
    char rb[RAMFS_FILE_SIZE];
    char entry_name[32];
    int len;
    int i;

    len = catalog_read_file(CATALOG_PATH_ROLLBACK, rb, (int)sizeof(rb));
    if (len < 0)
        return len;

    for (i = 0; i < len; i++) {
        int j = i;

        while (j < len && rb[j] != '\n')
            j++;
        rb[j] = '\0';
        if (split_field(rb + i, 0, entry_name, (int)sizeof(entry_name)) &&
            str_eq(entry_name, name)) {
            split_field(rb + i, 1, version, verlen);
            split_field(rb + i, 2, backup, backlen);
            return 0;
        }
        i = j;
    }
    return ENOENT;
}

void catalog_init(void) {
    kprintf("[catalog] package catalog ready\n");
}

int catalog_list(void) {
    char index[RAMFS_FILE_SIZE];
    int len;
    int i;
    int count = 0;

    len = catalog_read_file(CATALOG_PATH_INDEX, index, (int)sizeof(index));
    if (len < 0) {
        kprintf("[catalog] list: no index (%d)\n", len);
        return len;
    }

    kprintf("[catalog] entries:\n");
    for (i = 0; i < len; i++) {
        int j = i;
        char name[32];
        char version[16];
        char pkg[48];
        char install[48];
        char channel[8];
        char minver[16];

        while (j < len && index[j] != '\n')
            j++;
        index[j] = '\0';
        if (split_field(index + i, 0, name, (int)sizeof(name))) {
            split_field(index + i, 1, version, (int)sizeof(version));
            split_field(index + i, 2, pkg, (int)sizeof(pkg));
            split_field(index + i, 3, install, (int)sizeof(install));
            split_field(index + i, 4, channel, (int)sizeof(channel));
            split_field(index + i, 5, minver, (int)sizeof(minver));
            kprintf("[catalog]  %s %s pkg=%s install=%s channel=%s min=%s\n",
                    name, version, pkg, install, channel, minver);
            count++;
        }
        i = j;
    }
    kprintf("[catalog] list ok count=%d\n", count);
    return count;
}

int catalog_install(const char *name) {
    char kname[32];
    struct catalog_entry entry;
    char cur_ver[16];
    char backup[64];
    int len;
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_FS))
        return EPERM;
    if (!name)
        return EINVAL;

    len = user_strnlen(name, (int)sizeof(kname) - 1);
    if (len <= 0)
        return EINVAL;
    if (copy_from_user(kname, name, (unsigned long)len + 1) < 0)
        return EFAULT;

    rc = catalog_find_entry(kname, &entry);
    if (rc != 0) {
        kprintf("[catalog] install: entry not found name=%s\n", kname);
        return rc;
    }

    rc = catalog_active_version(kname, cur_ver, (int)sizeof(cur_ver));
    if (rc == 0) {
        if (!version_ge(cur_ver, entry.min_version)) {
            kprintf("[catalog] install: version constraint fail cur=%s min=%s\n",
                    cur_ver, entry.min_version);
            return EPERM;
        }
    } else if (entry.min_version[0] != '0') {
        kprintf("[catalog] install: no active version for min=%s\n",
                entry.min_version);
        return EPERM;
    }

    rc = ota_verify_package_kpath(entry.pkg_path);
    if (rc != 0) {
        kprintf("[catalog] install: verify failed rc=%d\n", rc);
        return rc;
    }

    backup[0] = '\0';
    for (len = 0; entry.install_path[len] && len < (int)sizeof(backup) - 5; len++)
        backup[len] = entry.install_path[len];
    backup[len++] = '.';
    backup[len++] = 'p';
    backup[len++] = 'r';
    backup[len++] = 'e';
    backup[len++] = 'v';
    backup[len] = '\0';

    if (catalog_file_size(entry.install_path) > 0) {
        if (catalog_active_version(kname, cur_ver, (int)sizeof(cur_ver)) == 0) {
            rc = catalog_set_rollback(kname, cur_ver, backup);
            if (rc != 0)
                return rc;
        }
        rc = catalog_copy_file(entry.install_path, backup);
        if (rc != 0) {
            kprintf("[catalog] install: backup failed rc=%d\n", rc);
            return rc;
        }
        kprintf("[catalog] backup -> %s\n", backup);
    }

    rc = ota_apply_package_kpath(entry.pkg_path);
    if (rc != 0) {
        kprintf("[catalog] install: apply failed rc=%d\n", rc);
        return rc;
    }

    rc = catalog_set_active(kname, entry.version, entry.install_path);
    if (rc != 0)
        return rc;

    kprintf("[catalog] install ok name=%s ver=%s\n", kname, entry.version);
    return 0;
}

int catalog_rollback(const char *name) {
    char kname[32];
    char prev_ver[16];
    char backup[64];
    char install[64];
    int len;
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_FS))
        return EPERM;
    if (!name)
        return EINVAL;

    len = user_strnlen(name, (int)sizeof(kname) - 1);
    if (len <= 0)
        return EINVAL;
    if (copy_from_user(kname, name, (unsigned long)len + 1) < 0)
        return EFAULT;

    rc = catalog_read_rollback(kname, prev_ver, (int)sizeof(prev_ver),
                              backup, (int)sizeof(backup));
    if (rc != 0) {
        kprintf("[catalog] rollback: no rollback point name=%s\n", kname);
        return rc;
    }

    if (catalog_file_size(backup) <= 0) {
        kprintf("[catalog] rollback: backup missing path=%s\n", backup);
        return ENOENT;
    }

    rc = catalog_active_install(kname, install, (int)sizeof(install));
    if (rc != 0)
        memcpy_local(install, backup, str_len(backup) + 1);

    rc = catalog_copy_file(backup, install);
    if (rc != 0) {
        kprintf("[catalog] rollback: restore failed rc=%d\n", rc);
        return rc;
    }

    rc = catalog_set_active(kname, prev_ver, install);
    if (rc != 0)
        return rc;

    ramfs_put(CATALOG_PATH_ROLLBACK, "", 0);
    kprintf("[catalog] rollback ok name=%s ver=%s\n", kname, prev_ver);
    return 0;
}
