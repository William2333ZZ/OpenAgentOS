#include "fleet.h"
#include "agent.h"
#include "printf.h"
#include "ramfs.h"
#include "quota.h"
#include "timer.h"
#include "../include/platform.h"
#include "../include/agentos.h"

#define FLEET_CACHE "/agent/0/fleet.json"

static int append_ulong(char *buf, int pos, int cap, unsigned long val) {
    char tmp[16];
    int i = 0;

    if (val == 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '0';
        return pos;
    }
    while (val > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = tmp[--i];
    }
    return pos;
}

static int append_int(char *buf, int pos, int cap, int val) {
    if (val < 0) {
        if (pos >= cap - 1)
            return -1;
        buf[pos++] = '-';
        return append_ulong(buf, pos, cap, (unsigned long)(-val));
    }
    return append_ulong(buf, pos, cap, (unsigned long)val);
}

void fleet_init(void) {
    kprintf("[fleet] telemetry ready cache=%s\n", FLEET_CACHE);
}

int fleet_status(struct agent *a) {
    int used = 0;
    int max_ipc = QUOTA_IPC_MAX;

    if (a) {
        used = quota_ipc_used(a);
        max_ipc = quota_ipc_max(a);
    }
    kprintf("[fleet] platform=%s agents=%d uptime=%lu quota_ipc=%d/%d\n",
            platform_name(), agent_count, timer_now(), used, max_ipc);
    return agent_count;
}

int fleet_push(struct agent *a) {
    char json[256];
    int pos = 0;
    int used = 0;
    int max_ipc = QUOTA_IPC_MAX;
    int rc;

    if (a) {
        used = quota_ipc_used(a);
        max_ipc = quota_ipc_max(a);
    }

    json[pos++] = '{';
    json[pos++] = '"';
    json[pos++] = 'v';
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '"';
    json[pos++] = '0';
    json[pos++] = '.';
    json[pos++] = '1';
    json[pos++] = '-';
    json[pos++] = 'b';
    json[pos++] = 'e';
    json[pos++] = 't';
    json[pos++] = 'a';
    json[pos++] = '"';
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'p';
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '"';
    {
        const char *p = platform_name();
        while (*p && pos < (int)sizeof(json) - 2) {
            json[pos++] = *p;
            p++;
        }
    }
    json[pos++] = '"';
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'a';
    json[pos++] = 'g';
    json[pos++] = 'e';
    json[pos++] = 'n';
    json[pos++] = 't';
    json[pos++] = 's';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), agent_count);
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'u';
    json[pos++] = 'p';
    json[pos++] = 't';
    json[pos++] = 'i';
    json[pos++] = 'm';
    json[pos++] = 'e';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_ulong(json, pos, (int)sizeof(json), timer_now());
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'q';
    json[pos++] = 'u';
    json[pos++] = 'o';
    json[pos++] = 't';
    json[pos++] = 'a';
    json[pos++] = '_';
    json[pos++] = 'i';
    json[pos++] = 'p';
    json[pos++] = 'c';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), used);
    if (pos < 0)
        return ENOSPC;
    json[pos++] = ',';
    json[pos++] = '"';
    json[pos++] = 'q';
    json[pos++] = 'u';
    json[pos++] = 'o';
    json[pos++] = 't';
    json[pos++] = 'a';
    json[pos++] = '_';
    json[pos++] = 'i';
    json[pos++] = 'p';
    json[pos++] = 'c';
    json[pos++] = '_';
    json[pos++] = 'm';
    json[pos++] = 'a';
    json[pos++] = 'x';
    json[pos++] = '"';
    json[pos++] = ':';
    pos = append_int(json, pos, (int)sizeof(json), max_ipc);
    if (pos < 0 || pos >= (int)sizeof(json) - 2)
        return ENOSPC;
    json[pos++] = '}';
    json[pos] = '\0';

    rc = ramfs_put(FLEET_CACHE, json, pos);
    if (rc < 0) {
        kprintf("[fleet] push failed rc=%d\n", rc);
        return rc;
    }
    kprintf("[fleet] push ok bytes=%d path=%s\n", pos, FLEET_CACHE);
    (void)a;
    return pos;
}

int fleet_probe(struct agent *a, const char *url) {
    if (!url || !url[0])
        return EINVAL;
    kprintf("[fleet] probe agent=%d url=%s (host collector stub)\n", a ? a->id : -1, url);
    return 0;
}
