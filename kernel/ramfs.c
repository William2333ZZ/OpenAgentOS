#include "ramfs.h"
#include "mem.h"
#include "printf.h"
#include "../include/agentos.h"

struct ramfs_file {
    char path[RAMFS_PATH_MAX];
    char data[RAMFS_FILE_SIZE];
    int size;
    int used;
};

static struct ramfs_file files[RAMFS_MAX_FILES];

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, int cap) {
    int i;
    for (i = 0; i < cap - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static struct ramfs_file *lookup(const char *path, int create) {
    int i;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].path, path))
            return &files[i];
    }
    if (!create)
        return 0;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            files[i].size = 0;
            str_copy(files[i].path, path, RAMFS_PATH_MAX);
            return &files[i];
        }
    }
    return 0;
}

static void ramfs_seed(const char *path, const char *content) {
    struct ramfs_file *f = lookup(path, 1);
    int i;
    if (!f)
        return;
    for (i = 0; content[i] && i < RAMFS_FILE_SIZE - 1; i++)
        f->data[i] = content[i];
    f->data[i] = '\0';
    f->size = i;
}

void ramfs_init(void) {
    int i;
    for (i = 0; i < RAMFS_MAX_FILES; i++)
        files[i].used = 0;
    ramfs_seed("/sys/version", "OpenAgentOS v1.0");
    ramfs_seed("/sys/tick_hz", "10000000");
    kprintf("[ramfs] seeded /sys/* (max %d files)\n", RAMFS_MAX_FILES);
}

int ramfs_read(const char *path, unsigned long offset, char *buf, int len) {
    struct ramfs_file *f;
    int i;
    if (!path || !buf || len <= 0)
        return EINVAL;
    f = lookup(path, 0);
    if (!f)
        return ENOENT;
    if ((unsigned long)offset >= (unsigned long)f->size)
        return 0;
    if (offset + (unsigned long)len > (unsigned long)f->size)
        len = f->size - (int)offset;
    for (i = 0; i < len; i++)
        buf[i] = f->data[offset + i];
    return len;
}

int ramfs_write(const char *path, unsigned long offset, const char *data, int len) {
    struct ramfs_file *f;
    int i;
    if (!path || !data || len <= 0)
        return EINVAL;
    f = lookup(path, 1);
    if (!f)
        return ENOSPC;
    if (offset + (unsigned long)len >= RAMFS_FILE_SIZE)
        return EINVAL;
    for (i = 0; i < len; i++)
        f->data[offset + i] = data[i];
    if (offset + len > f->size)
        f->size = (int)(offset + len);
    return len;
}

static int is_sys_path(const char *path) {
    return path[1] == 's' && path[2] == 'y' && path[3] == 's';
}

int ramfs_put(const char *path, const char *data, int size) {
    struct ramfs_file *f;
    int i;
    if (!path || !data || size <= 0 || size >= RAMFS_FILE_SIZE)
        return EINVAL;
    f = lookup(path, 1);
    if (!f)
        return ENOSPC;
    for (i = 0; i < size; i++)
        f->data[i] = data[i];
    f->data[size] = '\0';
    f->size = size;
    return 0;
}

int ramfs_path_exists(const char *path) {
    return lookup(path, 0) != 0;
}

int ramfs_count_agent_files(int agent_id) {
    char prefix[RAMFS_PATH_MAX];
    int pos = 0;
    int digits[8];
    int nd = 0;
    int n = agent_id;
    int plen;
    int i;
    int count = 0;

    prefix[pos++] = '/';
    prefix[pos++] = 'a';
    prefix[pos++] = 'g';
    prefix[pos++] = 'e';
    prefix[pos++] = 'n';
    prefix[pos++] = 't';
    prefix[pos++] = '/';
    if (n == 0) {
        prefix[pos++] = '0';
    } else {
        while (n > 0) {
            digits[nd++] = n % 10;
            n /= 10;
        }
        while (nd > 0)
            prefix[pos++] = '0' + digits[--nd];
    }
    prefix[pos++] = '/';
    prefix[pos] = '\0';
    plen = pos;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        int j;

        if (!files[i].used || is_sys_path(files[i].path))
            continue;
        for (j = 0; j < plen; j++) {
            if (files[i].path[j] != prefix[j])
                break;
        }
        if (j == plen)
            count++;
    }
    return count;
}

void ramfs_clear_user(void) {
    int i;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && !is_sys_path(files[i].path))
            files[i].used = 0;
    }
}

int ramfs_user_count(void) {
    int i;
    int n = 0;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && !is_sys_path(files[i].path))
            n++;
    }
    return n;
}

int ramfs_user_get(int idx, char *path, char *data, int cap) {
    int i;
    int seen = 0;
    if (!path || !data || cap <= 0)
        return EINVAL;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used || is_sys_path(files[i].path))
            continue;
        if (seen == idx) {
            str_copy(path, files[i].path, RAMFS_PATH_MAX);
            if (files[i].size >= cap)
                return EINVAL;
            for (int j = 0; j < files[i].size; j++)
                data[j] = files[i].data[j];
            return files[i].size;
        }
        seen++;
    }
    return ENOENT;
}
