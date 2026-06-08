#include "tool.h"
#include "agent.h"
#include "http.h"
#include "llm.h"
#include "mem.h"
#include "persist.h"
#include "printf.h"
#include "ramfs.h"
#include "session.h"
#include "timer.h"
#include "uaccess.h"
#include "smp.h"
#include "ota.h"
#include "catalog.h"
#include "tenant.h"
#include "audit.h"
#include "namespace.h"
#include "quota.h"
#include "fleet.h"
#include "policy.h"
#include "remote.h"
#include "mesh.h"
#include "virtio_gpu.h"
#include "virtio_input.h"
#include "uart.h"
#include "../include/agentos.h"

#define PATH_BUF  RAMFS_PATH_MAX
#define IO_BUF    256
#define IO_MAX    RAMFS_FILE_SIZE

struct tool_entry {
    int tool;
    unsigned int cap;
    int (*check)(struct agent *a, long arg0, long arg1, long arg2);
    int (*run)(struct agent *a, long arg0, long arg1, long arg2);
};

static int is_audit_path(const char *path) {
    return path[1] == 'a' && path[2] == 'u' && path[3] == 'd' && path[4] == 'i' &&
           path[5] == 't' && path[6] == '/';
}

static int append_int(char *buf, int pos, int cap, int val) {
    char tmp[12];
    int i = 0;
    unsigned int n;

    if (val < 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '-';
        n = (unsigned int)(-val);
    } else {
        n = (unsigned int)val;
    }
    if (n == 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '0';
        return pos;
    }
    while (n > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = tmp[--i];
    }
    return pos;
}

static void tool_audit_log(struct agent *a, int tool, int rc) {
    char line[32];
    int pos = 0;
    int wrc;

    line[pos++] = 't';
    line[pos++] = 'o';
    line[pos++] = 'o';
    line[pos++] = 'l';
    line[pos++] = '=';
    pos = append_int(line, pos, (int)sizeof(line), tool);
    if (pos < 0)
        return;
    line[pos++] = ' ';
    line[pos++] = 'r';
    line[pos++] = 'c';
    line[pos++] = '=';
    pos = append_int(line, pos, (int)sizeof(line), rc);
    if (pos < 0)
        return;
    line[pos++] = '\n';

    wrc = audit_write(a, line, pos);
    if (wrc == ENOSPC) {
        audit_compact(a, 8);
        wrc = audit_write(a, line, pos);
    }
    if (wrc < 0)
        kprintf("[tool:audit] agent=%d tool=%d rc=%d log_rc=%d\n", a->id, tool, rc, wrc);
}

static int policy_add(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    if (arg0 < -1000000 || arg0 > 1000000 || arg1 < -1000000 || arg1 > 1000000)
        return EINVAL;
    return 0;
}

static int run_add(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    return (int)(arg0 + arg1);
}

static int run_log(struct agent *a, long arg0, long arg1, long arg2) {
    (void)arg1;
    (void)arg2;
    kprintf("[tool:log] agent=%d val=%d\n", a->id, (int)arg0);
    return 0;
}

static int read_path_from_user(struct agent *a, long arg0, char *path) {
    (void)a;
    int len = user_strnlen((const char *)arg0, PATH_BUF - 1);
    if (len < 0)
        return EFAULT;
    if (len <= 0 || len >= PATH_BUF)
        return EINVAL;
    if (copy_from_user(path, (const void *)arg0, (unsigned long)len + 1) < 0)
        return EFAULT;
    return 0;
}

static int storage_path_ok(const char *path, int write) {
    if (path[0] != '/')
        return EINVAL;
    if (is_audit_path(path))
        return EPERM;
    if (namespace_is_path(path))
        return EPERM;
    if (path[1] == 's' && path[2] == 'y' && path[3] == 's')
        return write ? EPERM : 0;
    if (path[1] == 'a' && path[2] == 'g' && path[3] == 'e' && path[4] == 'n' &&
        path[5] == 't' && path[6] == '/')
        return 0;
    if (path[1] == 's' && path[2] == 'e' && path[3] == 's' && path[4] == 's' &&
        path[5] == 'i' && path[6] == 'o' && path[7] == 'n' && path[8] == '/')
        return 0;
    return EPERM;
}

static int policy_fs_read(struct agent *a, long arg0, long arg1, long arg2) {
    char path[PATH_BUF];
    (void)arg1;
    (void)arg2;
    if (read_path_from_user(a, arg0, path) != 0)
        return EINVAL;
    if (is_audit_path(path))
        return audit_path_ok(a, path) ? 0 : EPERM;
    if (namespace_is_path(path))
        return namespace_path_ok(a, path) ? 0 : EPERM;
    if (agent_has_cap(a, CAP_SVC_STORAGE))
        return storage_path_ok(path, 0);
    if (path[0] != '/')
        return EINVAL;
    if (path[1] == 's' && path[2] == 'y' && path[3] == 's')
        return 0;
    if (path[1] == 's' && path[2] == 'e' && path[3] == 's' && path[4] == 's' &&
        path[5] == 'i' && path[6] == 'o' && path[7] == 'n' && path[8] == '/') {
        return tenant_session_path_ok(a, path) ? 0 : EPERM;
    }
    if (path[1] == 'a' && path[2] == 'g' && path[3] == 'e' && path[4] == 'n' &&
        path[5] == 't' && path[6] == '/') {
        int id = 0;
        int i = 7;
        while (path[i] >= '0' && path[i] <= '9') {
            id = id * 10 + (path[i] - '0');
            i++;
        }
        if (path[i] != '/')
            return EINVAL;
        return id == a->id ? 0 : EPERM;
    }
    return EPERM;
}

static int policy_fs_write(struct agent *a, long arg0, long arg1, long arg2) {
    char path[PATH_BUF];
    (void)arg1;
    (void)arg2;
    if (read_path_from_user(a, arg0, path) != 0)
        return EINVAL;
    if (is_audit_path(path))
        return EPERM;
    if (namespace_is_path(path))
        return namespace_path_ok(a, path) ? 0 : EPERM;
    if (agent_has_cap(a, CAP_SVC_STORAGE))
        return storage_path_ok(path, 1);
    if (path[1] == 's' && path[2] == 'y' && path[3] == 's')
        return EPERM;
    if (path[1] == 'a' && path[2] == 'g' && path[3] == 'e' && path[4] == 'n' &&
        path[5] == 't' && path[6] == '/') {
        int id = 0;
        int i = 7;
        while (path[i] >= '0' && path[i] <= '9') {
            id = id * 10 + (path[i] - '0');
            i++;
        }
        if (path[i] != '/')
            return EINVAL;
        return id == a->id ? 0 : EPERM;
    }
    return EPERM;
}

static int run_read(struct agent *a, long arg0, long arg1, long arg2) {
    char path[PATH_BUF];
    char small[IO_BUF];
    char *kbuf = small;
    int len = (int)arg2;
    int rc;
    int heap = 0;

    if (read_path_from_user(a, arg0, path) != 0)
        return EINVAL;
    if (len <= 0 || len > IO_MAX)
        return EINVAL;
    if (len > IO_BUF) {
        kbuf = kalloc((unsigned long)len);
        if (!kbuf)
            return ENOSPC;
        heap = 1;
    }
    rc = ramfs_read(path, 0, kbuf, len);
    if (rc < 0) {
        if (heap)
            kfree(kbuf);
        return rc;
    }
    if (copy_to_user((void *)arg1, kbuf, (unsigned long)rc) < 0) {
        if (heap)
            kfree(kbuf);
        return EFAULT;
    }
    kbuf[rc] = '\0';
    kprintf("[tool:read] agent=%d path=%s len=%d\n", a->id, path, rc);
    if (heap)
        kfree(kbuf);
    return rc;
}

static int run_write(struct agent *a, long arg0, long arg1, long arg2) {
    char path[PATH_BUF];
    char small[IO_BUF];
    char *kbuf = small;
    int len = (int)arg2;
    int rc;
    int heap = 0;

    if (read_path_from_user(a, arg0, path) != 0)
        return EINVAL;
    if (len <= 0 || len > IO_MAX)
        return EINVAL;
    if (len > IO_BUF) {
        kbuf = kalloc((unsigned long)len);
        if (!kbuf)
            return ENOSPC;
        heap = 1;
    }
    if (copy_from_user(kbuf, (const void *)arg1, (unsigned long)len) < 0) {
        if (heap)
            kfree(kbuf);
        return EFAULT;
    }
    rc = quota_fs_allow_new(a, path);
    if (rc != 0)
        return rc;
    rc = ramfs_write(path, 0, kbuf, len);
    if (heap)
        kfree(kbuf);
    if (rc < 0)
        return rc;
    kprintf("[tool:write] agent=%d path=%s len=%d\n", a->id, path, rc);
    return rc;
}

static int run_time(struct agent *a, long arg0, long arg1, long arg2) {
    uint64_t t;
    (void)a;
    (void)arg1;
    (void)arg2;
#ifdef PLATFORM_X86_64_PC
    extern unsigned long timer_now(void);
    t = timer_now();
#else
    __asm__ volatile("csrr %0, time" : "=r"(t));
#endif
    if (arg0) {
        if (copy_to_user((void *)arg0, &t, sizeof(t)) < 0)
            return EFAULT;
    }
    return (int)(t & 0x7fffffffUL);
}

static int policy_llm(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    if (!arg0 || !arg1 || arg2 <= 0)
        return EINVAL;
    return 0;
}

static int run_llm(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    return agent_llm_query((const char *)arg0, (char *)arg1, (int)arg2);
}

static int policy_session(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg1;
    (void)arg2;
    if (!arg0)
        return EINVAL;
    return 0;
}

static int run_session_append(struct agent *a, long arg0, long arg1, long arg2) {
    char text[128];
    int len;
    (void)arg1;
    (void)arg2;
    len = user_strnlen((const char *)arg0, sizeof(text) - 1);
    if (len < 0)
        return EFAULT;
    if (len <= 0)
        return EINVAL;
    if (copy_from_user(text, (const void *)arg0, (unsigned long)len + 1) < 0)
        return EFAULT;
    return session_append(a, text);
}

static int run_session_tail(struct agent *a, long arg0, long arg1, long arg2) {
    char kbuf[128];
    int buflen = (int)arg1;
    int rc;
    (void)arg2;
    if (!arg0 || buflen <= 0 || buflen > (int)sizeof(kbuf))
        return EINVAL;
    rc = session_tail(a, kbuf, buflen);
    if (rc < 0)
        return rc;
    if (copy_to_user((void *)arg0, kbuf, (unsigned long)rc + 1) < 0)
        return EFAULT;
    return rc;
}

static int run_session_read(struct agent *a, long arg0, long arg1, long arg2) {
    char kbuf[RAMFS_FILE_SIZE];
    int buflen = (int)arg1;
    int max_lines = (int)arg2;
    int rc;

    if (!arg0 || buflen <= 0 || buflen > (int)sizeof(kbuf))
        return EINVAL;
    rc = session_read(a, kbuf, buflen, max_lines);
    if (rc < 0)
        return rc;
    if (copy_to_user((void *)arg0, kbuf, (unsigned long)rc + 1) < 0)
        return EFAULT;
    return rc;
}

static int run_session_compact(struct agent *a, long arg0, long arg1, long arg2) {
    (void)arg1;
    (void)arg2;
    return session_compact(a, (int)arg0);
}

static int policy_gpio(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    if (arg0 < 0 || arg0 > 63)
        return EINVAL;
    if (arg1 != 0 && arg1 != 1)
        return EINVAL;
    return 0;
}

static int run_gpio(struct agent *a, long arg0, long arg1, long arg2) {
    (void)arg2;
    kprintf("[tool:gpio] agent=%d pin=%d val=%d\n", a->id, (int)arg0, (int)arg1);
    return (int)arg1;
}

static int policy_http(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    if (!arg0 || !arg1 || arg2 <= 0)
        return EINVAL;
    return 0;
}

static int run_http(struct agent *a, long arg0, long arg1, long arg2) {
    char url[HTTP_MAX_URL];
    char body[HTTP_MAX_BODY];
    int buflen = (int)arg2;
    int ulen;
    int rc;

    (void)a;
    if (!arg0 || !arg1 || buflen <= 0 || buflen > HTTP_MAX_BODY)
        return EINVAL;
    ulen = user_strnlen((const char *)arg0, (int)sizeof(url) - 1);
    if (ulen <= 0)
        return EINVAL;
    if (copy_from_user(url, (const void *)arg0, (unsigned long)ulen + 1) < 0)
        return EFAULT;

    rc = agent_http_fetch(url, body, (int)sizeof(body));
    if (rc < 0)
        return rc;
    if (rc >= buflen)
        return EINVAL;
    if (copy_to_user((void *)arg1, body, (unsigned long)rc + 1) < 0)
        return EFAULT;
    kprintf("[tool:http] agent=%d url=%s len=%d\n", a->id, url, rc);
    return rc;
}

static int policy_display(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    return 0;
}

static int run_display(struct agent *a, long arg0, long arg1, long arg2) {
    char text[128];
    int cmd = (int)arg0;
    int x;
    int y;

    (void)a;
    if (cmd == 0) {
        if (virtio_gpu_ready())
            virtio_gpu_clear((uint32_t)arg1);
        return 0;
    }
    if (cmd == 2) {
        if (virtio_gpu_ready())
            return virtio_gpu_flush();
        return 0;
    }
    if (cmd != 1)
        return EINVAL;

    x = (int)((arg1 >> 16) & 0xffff);
    y = (int)(arg1 & 0xffff);
    if (!arg2)
        return EINVAL;
    if (user_strnlen((const char *)arg2, (int)sizeof(text) - 1) <= 0)
        return EINVAL;
    if (copy_from_user(text, (const void *)arg2, sizeof(text)) < 0)
        return EFAULT;

    if (virtio_gpu_ready()) {
        virtio_gpu_draw_text(x, y, text, 0xffeeeeee, 0xff102030);
        return 0;
    }

    kprintf("[ui-console] (%d,%d) %s\n", x, y, text);
    return 0;
}

static int policy_input(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    return 0;
}

static int run_input(struct agent *a, long arg0, long arg1, long arg2) {
    int type;
    int code;
    int value;
    int rc;
    char ch;

    (void)a;
    (void)arg1;
    (void)arg2;
    if ((int)arg0 != 0)
        return EINVAL;

    if (virtio_input_ready()) {
        rc = virtio_input_poll(&type, &code, &value);
        if (rc <= 0)
            return rc;
        if (type == INPUT_EV_KEY && value == 1)
            return code;
        return 0;
    }

    if (!uart_poll_char(&ch))
        return 0;
    if (ch == 'h' || ch == 'H')
        return 11;
    if (ch == '\r' || ch == '\n')
        return 0;
    kprintf("[ui-console] uart key '%c'\n", ch);
    return (int)(unsigned char)ch;
}

static int policy_console(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    return 0;
}

static int run_console(struct agent *a, long arg0, long arg1, long arg2) {
    char kbuf[CONSOLE_MAX_LINE + 4];
    char ch;
    int cmd = (int)arg0;
    int rc;

    (void)a;
    switch (cmd) {
    case CONSOLE_CMD_READ_CHAR:
        if (!uart_poll_char(&ch))
            return 0;
        return (int)(unsigned char)ch;
    case CONSOLE_CMD_GETLINE:
        if (!arg1 || arg2 <= 0)
            return EINVAL;
        rc = uart_getline(kbuf, arg2 < (long)sizeof(kbuf) ? (int)arg2 : (int)sizeof(kbuf));
        if (rc < 0)
            return rc;
        if (copy_to_user((void *)arg1, kbuf, (unsigned long)rc + 1) < 0)
            return EFAULT;
        return rc;
    case CONSOLE_CMD_PUTS:
        if (!arg1)
            return EINVAL;
        rc = user_strnlen((const char *)arg1, CONSOLE_MAX_LINE);
        if (rc <= 0)
            return EINVAL;
        if (copy_from_user(kbuf, (const void *)arg1, rc + 1) < 0)
            return EFAULT;
        uart_puts(kbuf);
        return 0;
    case CONSOLE_CMD_PROMPT:
        if (arg1) {
            rc = user_strnlen((const char *)arg1, 63);
            if (rc > 0) {
                if (copy_from_user(kbuf, (const void *)arg1, rc + 1) < 0)
                    return EFAULT;
                uart_puts(kbuf);
                return 0;
            }
        }
        uart_puts("agentos> ");
        return 0;
    default:
        return EINVAL;
    }
}

#define SMP_TOOL_ONLINE 0
#define SMP_TOOL_TICKS  1
#define SMP_TOOL_HART   2

static int run_smp(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    switch ((int)arg0) {
    case SMP_TOOL_ONLINE:
        return smp_online_count();
    case SMP_TOOL_TICKS:
        return (int)smp_hart_ticks((int)arg1);
    case SMP_TOOL_HART:
        return smp_hart_id();
    default:
        return EINVAL;
    }
}

static int run_ota(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    switch ((int)arg0) {
    case OTA_CMD_APPLY:
        return ota_apply_package((const char *)arg1);
    case OTA_CMD_VERIFY:
        return ota_verify_package((const char *)arg1);
    case OTA_CMD_CHANNEL_GET:
        return ota_channel_get((char *)arg1, (int)arg2);
    case OTA_CMD_CHANNEL_SET:
        return ota_channel_set((const char *)arg1);
    case OTA_CMD_KERNEL_GEN:
        return ota_kernel_gen();
    default:
        return EINVAL;
    }
}

static int run_catalog(struct agent *a, long arg0, long arg1, long arg2) {
    (void)a;
    (void)arg2;
    switch ((int)arg0) {
    case CATALOG_CMD_LIST:
        return catalog_list();
    case CATALOG_CMD_INSTALL:
        return catalog_install((const char *)arg1);
    case CATALOG_CMD_ROLLBACK:
        return catalog_rollback((const char *)arg1);
    default:
        return EINVAL;
    }
}

static int run_audit(struct agent *a, long arg0, long arg1, long arg2) {
    int used;
    int quota;
    int rc;

    (void)arg2;
    switch ((int)arg0) {
    case AUDIT_CMD_STATUS:
        used = audit_usage(a);
        quota = audit_quota(a);
        kprintf("[audit] agent=%d used=%d quota=%d\n", a->id, used, quota);
        return used;
    case AUDIT_CMD_TAIL:
        return audit_tail(a, (int)arg1);
    case AUDIT_CMD_PROBE:
        rc = audit_probe_cross_read(a, (int)arg1);
        kprintf("[audit] probe agent=%d target=%d rc=%d\n",
                a->id, (int)arg1, rc);
        return rc;
    case AUDIT_CMD_COMPACT:
        return audit_compact(a, (int)arg1);
    default:
        return EINVAL;
    }
}

static int run_namespace(struct agent *a, long arg0, long arg1, long arg2) {
    char text[128];
    int len;
    int rc;

    (void)arg2;
    switch ((int)arg0) {
    case NS_CMD_STATUS:
        return namespace_status(a);
    case NS_CMD_PROBE:
        rc = namespace_probe_cross_read(a, (int)arg1);
        kprintf("[namespace] probe agent=%d target=%d rc=%d\n",
                a->id, (int)arg1, rc);
        return rc;
    case NS_CMD_WRITE:
        len = user_strnlen((const char *)arg1, sizeof(text) - 1);
        if (len < 0)
            return EFAULT;
        if (len <= 0)
            return EINVAL;
        if (copy_from_user(text, (const void *)arg1, (unsigned long)len + 1) < 0)
            return EFAULT;
        return namespace_write_secret(a, text, len);
    default:
        return EINVAL;
    }
}

static int run_quota(struct agent *a, long arg0, long arg1, long arg2) {
    char text[128];
    int len;

    (void)arg2;
    switch ((int)arg0) {
    case QUOTA_CMD_STATUS:
        return quota_status(a);
    case QUOTA_CMD_BURN:
        return quota_burn_ipc(a, (int)arg1);
    case QUOTA_CMD_PROBE:
        return quota_probe_ipc(a);
    case QUOTA_CMD_NOTIFY:
        len = user_strnlen((const char *)arg1, sizeof(text) - 1);
        if (len < 0)
            return EFAULT;
        if (len <= 0)
            return EINVAL;
        if (copy_from_user(text, (const void *)arg1, (unsigned long)len + 1) < 0)
            return EFAULT;
        return quota_notify(a, text);
    default:
        return EINVAL;
    }
}

static int run_fleet(struct agent *a, long arg0, long arg1, long arg2) {
    char url[128];
    int len;

    (void)arg2;
    switch ((int)arg0) {
    case FLEET_CMD_STATUS:
        return fleet_status(a);
    case FLEET_CMD_PUSH:
        return fleet_push(a);
    case FLEET_CMD_PROBE:
        len = user_strnlen((const char *)arg1, sizeof(url) - 1);
        if (len < 0)
            return EFAULT;
        if (len <= 0)
            return EINVAL;
        if (copy_from_user(url, (const void *)arg1, (unsigned long)len + 1) < 0)
            return EFAULT;
        return fleet_probe(a, url);
    default:
        return EINVAL;
    }
}

static int run_policy(struct agent *a, long arg0, long arg1, long arg2) {
    char path[128];
    int len;

    (void)arg2;
    switch ((int)arg0) {
    case POLICY_CMD_STATUS:
        return policy_status(a);
    case POLICY_CMD_LOAD:
        if (!arg1)
            return policy_load(a, 0);
        len = user_strnlen((const char *)arg1, sizeof(path) - 1);
        if (len < 0)
            return EFAULT;
        if (copy_from_user(path, (const void *)arg1, (unsigned long)len + 1) < 0)
            return EFAULT;
        return policy_load(a, path);
    case POLICY_CMD_PROBE:
        return policy_probe_tool(a, (int)arg1);
    case POLICY_CMD_DENY:
        return policy_deny_tool(a, (int)arg1);
    case POLICY_CMD_ALLOW:
        return policy_allow_tool(a, (int)arg1);
    default:
        return EINVAL;
    }
}

static int run_remote(struct agent *a, long arg0, long arg1, long arg2) {
    (void)arg1;
    (void)arg2;
    switch ((int)arg0) {
    case REMOTE_CMD_STATUS:
        return remote_status(a);
    case REMOTE_CMD_ENABLE:
        return remote_enable(a);
    case REMOTE_CMD_DISABLE:
        return remote_disable(a);
    default:
        return EINVAL;
    }
}

static int run_mesh(struct agent *a, long arg0, long arg1, long arg2) {
    char service[64];
    int len;

    (void)arg2;
    switch ((int)arg0) {
    case MESH_CMD_STATUS:
        return mesh_status(a);
    case MESH_CMD_BEACON:
        return mesh_beacon(a);
    case MESH_CMD_PROBE:
        len = user_strnlen((const char *)arg1, sizeof(service) - 1);
        if (len < 0)
            return EFAULT;
        if (len <= 0)
            return EINVAL;
        if (copy_from_user(service, (const void *)arg1, (unsigned long)len + 1) < 0)
            return EFAULT;
        return mesh_probe(a, service);
    default:
        return EINVAL;
    }
}

static int run_tenant(struct agent *a, long arg0, long arg1, long arg2) {
    int used;
    int quota;
    int rc;

    (void)arg2;
    switch ((int)arg0) {
    case TENANT_CMD_STATUS:
        used = tenant_session_usage(a);
        quota = tenant_session_quota(a);
        kprintf("[tenant] agent=%d session used=%d quota=%d\n",
                a->id, used, quota);
        return used;
    case TENANT_CMD_PROBE:
        rc = tenant_probe_cross_read(a, (int)arg1);
        kprintf("[tenant] probe agent=%d target=%d rc=%d\n",
                a->id, (int)arg1, rc);
        return rc;
    default:
        return EINVAL;
    }
}

static const struct tool_entry tool_table[] = {
    { TOOL_LOG,   CAP_LOG,  0,       run_log },
    { TOOL_ADD,   CAP_MATH, policy_add, run_add },
    { TOOL_TIME,  CAP_TIME, 0,       run_time },
    { TOOL_READ,  CAP_FS,   policy_fs_read, run_read },
    { TOOL_WRITE, CAP_FS,   policy_fs_write, run_write },
    { TOOL_LLM,   CAP_LLM,  policy_llm, run_llm },
    { TOOL_SESSION_APPEND, CAP_LOG, policy_session, run_session_append },
    { TOOL_SESSION_TAIL,   CAP_LOG, policy_session, run_session_tail },
    { TOOL_SESSION_READ,   CAP_LOG, policy_session, run_session_read },
    { TOOL_SESSION_COMPACT, CAP_LOG, policy_session, run_session_compact },
    { TOOL_GPIO,  CAP_GPIO, policy_gpio, run_gpio },
    { TOOL_HTTP,  CAP_NET,  policy_http, run_http },
    { TOOL_DISPLAY, CAP_DISPLAY, policy_display, run_display },
    { TOOL_INPUT,   CAP_INPUT,   policy_input, run_input },
    { TOOL_CONSOLE, CAP_SVC_CONSOLE, policy_console, run_console },
    { TOOL_SMP,   CAP_LOG,  0,       run_smp },
    { TOOL_OTA,   CAP_FS,   0,       run_ota },
    { TOOL_CATALOG, CAP_FS, 0,       run_catalog },
    { TOOL_TENANT,  CAP_LOG, 0,       run_tenant },
    { TOOL_AUDIT,   CAP_LOG, 0,       run_audit },
    { TOOL_NAMESPACE, CAP_LOG, 0,     run_namespace },
    { TOOL_QUOTA,     CAP_LOG, 0,     run_quota },
    { TOOL_FLEET,     CAP_LOG, 0,     run_fleet },
    { TOOL_POLICY,    CAP_LOG, 0,     run_policy },
    { TOOL_REMOTE,    CAP_LOG, 0,     run_remote },
    { TOOL_MESH,      CAP_LOG, 0,     run_mesh },
};

void tool_init(void) {
    kprintf("[tool] gateway %d entries (policy + dispatch)\n",
            (int)(sizeof(tool_table) / sizeof(tool_table[0])));
}

int tool_dispatch(struct agent *a, int tool, long arg0, long arg1, long arg2) {
    int i;
    int rc;
    if (!a)
        return EPERM;

    if (tool != TOOL_QUOTA &&
        tool != TOOL_POLICY &&
        tool != TOOL_FLEET &&
        tool != TOOL_REMOTE &&
        tool != TOOL_MESH &&
        !(tool == TOOL_CONSOLE && (arg0 == CONSOLE_CMD_READ_CHAR ||
                                  arg0 == CONSOLE_CMD_PUTS ||
                                  arg0 == CONSOLE_CMD_PROMPT))) {
        rc = quota_tool_allow(a);
        if (rc != 0) {
            tool_audit_log(a, tool, rc);
            return rc;
        }
    }

    if (tool != TOOL_POLICY &&
        tool != TOOL_FLEET &&
        tool != TOOL_REMOTE &&
        tool != TOOL_MESH &&
        tool != TOOL_QUOTA &&
        !(tool == TOOL_CONSOLE && (arg0 == CONSOLE_CMD_READ_CHAR ||
                                  arg0 == CONSOLE_CMD_PUTS ||
                                  arg0 == CONSOLE_CMD_PROMPT))) {
        rc = policy_tool_allow(a, tool);
        if (rc != 0) {
            tool_audit_log(a, tool, rc);
            return rc;
        }
    }

    for (i = 0; i < (int)(sizeof(tool_table) / sizeof(tool_table[0])); i++) {
        const struct tool_entry *e = &tool_table[i];
        if (e->tool != tool)
            continue;
        if (!agent_has_cap(a, e->cap)) {
            rc = EPERM;
            tool_audit_log(a, tool, rc);
            return rc;
        }
        if (e->check) {
            rc = e->check(a, arg0, arg1, arg2);
            if (rc != 0) {
                tool_audit_log(a, tool, rc);
                return rc;
            }
        }
        rc = e->run(a, arg0, arg1, arg2);
        if (!(tool == TOOL_INPUT && rc == 0) &&
            !(tool == TOOL_CONSOLE && (arg0 == CONSOLE_CMD_READ_CHAR ||
                                       arg0 == CONSOLE_CMD_PUTS ||
                                       arg0 == CONSOLE_CMD_PROMPT)) &&
            tool != TOOL_AUDIT &&
            tool != TOOL_NAMESPACE &&
            tool != TOOL_QUOTA &&
            tool != TOOL_FLEET &&
            tool != TOOL_POLICY &&
            tool != TOOL_REMOTE &&
            tool != TOOL_MESH)
            tool_audit_log(a, tool, rc);
        if (tool != TOOL_QUOTA &&
            tool != TOOL_FLEET &&
            tool != TOOL_POLICY &&
            tool != TOOL_REMOTE &&
            tool != TOOL_MESH &&
            !(tool == TOOL_CONSOLE && (arg0 == CONSOLE_CMD_READ_CHAR ||
                                       arg0 == CONSOLE_CMD_PUTS ||
                                       arg0 == CONSOLE_CMD_PROMPT)))
            quota_tool_charge(a);
        return rc;
    }
    rc = EINVAL;
    tool_audit_log(a, tool, rc);
    return rc;
}
