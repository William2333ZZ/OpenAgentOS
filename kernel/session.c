#include "session.h"
#include "tenant.h"
#include "ramfs.h"
#include "ipc.h"
#include "printf.h"
#include "../include/agentos.h"

#define SESSION_PATH_MAX 64
#define SESSION_LINE_MAX 192

static void build_session_path(struct agent *a, char *path) {
    int pos = 0;
    int id = a->id;
    int digits[8];
    int nd = 0;

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
            digits[nd++] = id % 10;
            id /= 10;
        }
        while (nd > 0)
            path[pos++] = '0' + digits[--nd];
    }
    path[pos++] = '/';
    path[pos++] = 'l';
    path[pos++] = 'o';
    path[pos++] = 'g';
    path[pos] = '\0';
}

static int str_len_local(const char *s, int max) {
    int n = 0;
    while (n < max && s[n])
        n++;
    return n;
}

int session_append(struct agent *a, const char *text) {
    char path[SESSION_PATH_MAX];
    char buf[RAMFS_FILE_SIZE];
    char line[SESSION_LINE_MAX];
    int old_len;
    int text_len;
    int line_len;
    int i;

    if (!a || !text)
        return EINVAL;

    build_session_path(a, path);
    text_len = str_len_local(text, SESSION_LINE_MAX - 4);
    if (text_len <= 0)
        return EINVAL;

    old_len = ramfs_read(path, 0, buf, RAMFS_FILE_SIZE - 2);
    if (old_len < 0) {
        if (old_len != ENOENT)
            return old_len;
        old_len = 0;
    }

    line_len = 0;
    for (i = 0; i < text_len; i++)
        line[line_len++] = text[i];
    line[line_len++] = '\n';

    if (tenant_session_allow(a, old_len + line_len) != 0)
        return ENOSPC;

    if (old_len + line_len >= RAMFS_FILE_SIZE)
        return ENOSPC;

    for (i = 0; i < line_len; i++)
        buf[old_len + i] = line[i];

    if (ramfs_write(path, 0, buf, old_len + line_len) < 0)
        return EIO;

    kprintf("[session] agent=%d append len=%d\n", a->id, line_len);
    return line_len;
}

int session_tail(struct agent *a, char *buf, int buflen) {
    char path[SESSION_PATH_MAX];
    char data[RAMFS_FILE_SIZE];
    int len;
    int start;
    int i;

    if (!a || !buf || buflen <= 0)
        return EINVAL;

    build_session_path(a, path);
    len = ramfs_read(path, 0, data, RAMFS_FILE_SIZE - 1);
    if (len < 0) {
        if (len == ENOENT) {
            buf[0] = '\0';
            return 0;
        }
        return len;
    }
    if (len == 0) {
        buf[0] = '\0';
        return 0;
    }
    data[len] = '\0';

    start = len - 1;
    while (start > 0 && data[start - 1] != '\n')
        start--;

    i = 0;
    while (data[start + i] && data[start + i] != '\n' && i < buflen - 1) {
        buf[i] = data[start + i];
        i++;
    }
    buf[i] = '\0';
    return i;
}

static int count_lines(const char *data, int len) {
    int lines = 0;
    int i;

    if (len <= 0)
        return 0;
    for (i = 0; i < len; i++) {
        if (data[i] == '\n')
            lines++;
    }
    if (data[len - 1] != '\n')
        lines++;
    return lines;
}

static int line_start_at(const char *data, int len, int line_idx) {
    int line = 0;
    int i = 0;

    if (line_idx <= 0)
        return 0;
    while (i < len && line < line_idx) {
        while (i < len && data[i] != '\n')
            i++;
        if (i < len)
            i++;
        line++;
    }
    return i;
}

int session_read(struct agent *a, char *buf, int buflen, int max_lines) {
    char path[SESSION_PATH_MAX];
    char data[RAMFS_FILE_SIZE];
    int len;
    int start;
    int i;

    if (!a || !buf || buflen <= 0)
        return EINVAL;

    build_session_path(a, path);
    len = ramfs_read(path, 0, data, RAMFS_FILE_SIZE - 1);
    if (len < 0) {
        if (len == ENOENT) {
            buf[0] = '\0';
            return 0;
        }
        return len;
    }
    if (len == 0) {
        buf[0] = '\0';
        return 0;
    }
    data[len] = '\0';

    start = 0;
    if (max_lines > 0) {
        int total = count_lines(data, len);
        if (total > max_lines)
            start = line_start_at(data, len, total - max_lines);
    }

    i = 0;
    while (start + i < len && data[start + i] && i < buflen - 1) {
        buf[i] = data[start + i];
        i++;
    }
    buf[i] = '\0';
    kprintf("[session] agent=%d read lines=%d len=%d\n", a->id, max_lines, i);
    return i;
}

int session_compact(struct agent *a, int keep) {
    char path[SESSION_PATH_MAX];
    char data[RAMFS_FILE_SIZE];
    char newbuf[RAMFS_FILE_SIZE];
    char summary[MSG_PAYLOAD_SIZE];
    struct agent_msg msg;
    int len;
    int total;
    int drop;
    int start;
    int sum_pos = 0;
    int out = 0;
    int i;

    if (!a || keep < 0)
        return EINVAL;

    build_session_path(a, path);
    len = ramfs_read(path, 0, data, RAMFS_FILE_SIZE - 1);
    if (len < 0) {
        if (len == ENOENT)
            return 0;
        return len;
    }
    if (len == 0)
        return 0;
    data[len] = '\0';

    total = count_lines(data, len);
    if (total <= keep)
        return 0;

    drop = total - keep;
    start = line_start_at(data, len, drop);

    summary[sum_pos++] = 's';
    summary[sum_pos++] = 'u';
    summary[sum_pos++] = 'm';
    summary[sum_pos++] = 'm';
    summary[sum_pos++] = 'a';
    summary[sum_pos++] = 'r';
    summary[sum_pos++] = 'y';
    summary[sum_pos++] = ':';
    summary[sum_pos++] = ' ';
    for (i = 0; i < start && sum_pos < (int)sizeof(summary) - 2; i++) {
        if (data[i] == '\n')
            summary[sum_pos++] = ';';
        else
            summary[sum_pos++] = data[i];
    }
    summary[sum_pos] = '\0';

    for (i = 0; summary[i] && out < (int)sizeof(newbuf) - 2; i++)
        newbuf[out++] = summary[i];
    newbuf[out++] = '\n';
    for (i = start; i < len && out < (int)sizeof(newbuf) - 1; i++)
        newbuf[out++] = data[i];
    newbuf[out] = '\0';

    if (ramfs_write(path, 0, newbuf, out) < 0)
        return EIO;

    msg.sender_id = 0;
    msg.type = MSG_SUMMARY;
    for (i = 0; i < MSG_PAYLOAD_SIZE; i++)
        msg.payload[i] = 0;
    for (i = 0; summary[i] && i < MSG_PAYLOAD_SIZE - 1; i++)
        msg.payload[i] = summary[i];

    if (msgbox_push(&a->inbox, &msg) != 0)
        return ENOSPC;

    kprintf("[session] agent=%d compact keep=%d dropped=%d\n", a->id, keep, drop);
    return drop;
}
