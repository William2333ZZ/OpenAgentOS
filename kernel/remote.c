#include "remote.h"
#include "printf.h"
#include "ramfs.h"
#include "netstack.h"

#define REMOTE_TOKEN_PATH "/sys/console/token"
#define REMOTE_DEFAULT_TOKEN "agentos-beta"

static int remote_enabled;

static int resp_has_pong(const char *resp, int n) {
    int i;
    if (n <= 0)
        return 0;
    for (i = 0; i + 3 < n; i++) {
        if (resp[i] == 'P' && resp[i + 1] == 'O' && resp[i + 2] == 'N' &&
            resp[i + 3] == 'G')
            return 1;
    }
    return 0;
}

void remote_init(void) {
    remote_enabled = 0;
    ramfs_put(REMOTE_TOKEN_PATH, REMOTE_DEFAULT_TOKEN,
              (int)sizeof(REMOTE_DEFAULT_TOKEN) - 1);
    kprintf("[remote] ready port=%d token_path=%s (tcp via slirp)\n",
            REMOTE_DEFAULT_PORT, REMOTE_TOKEN_PATH);
}

int remote_status(struct agent *a) {
    kprintf("[remote] agent=%d enabled=%d port=%d net=%d token=%s\n",
            a ? a->id : -1, remote_enabled, REMOTE_DEFAULT_PORT,
            netstack_ready(), REMOTE_DEFAULT_TOKEN);
    return remote_enabled;
}

int remote_enable(struct agent *a) {
    remote_enabled = 1;
    kprintf("[remote] enable agent=%d port=%d gateway=10.0.2.2\n",
            a ? a->id : -1, REMOTE_DEFAULT_PORT);
    return 0;
}

int remote_disable(struct agent *a) {
    remote_enabled = 0;
    kprintf("[remote] disable agent=%d\n", a ? a->id : -1);
    return 0;
}

int remote_ping(struct agent *a) {
    if (!remote_enabled) {
        kprintf("[remote] ping denied agent=%d (disabled)\n", a ? a->id : -1);
        return EPERM;
    }
    kprintf("[remote] ping ok agent=%d port=%d token=%s\n",
            a ? a->id : -1, REMOTE_DEFAULT_PORT, REMOTE_DEFAULT_TOKEN);
    return 0;
}

int remote_connect(struct agent *a) {
    char msg[64];
    char resp[64];
    int pos = 0;
    int n;
    int i;

    if (!remote_enabled) {
        kprintf("[remote] connect denied agent=%d (disabled)\n", a ? a->id : -1);
        return EPERM;
    }
    if (!netstack_ready()) {
        kprintf("[remote] connect no net agent=%d\n", a ? a->id : -1);
        return ENODEV;
    }

    msg[pos++] = 'L';
    msg[pos++] = 'I';
    msg[pos++] = 'N';
    msg[pos++] = 'K';
    msg[pos++] = ' ';
    for (i = 0; REMOTE_DEFAULT_TOKEN[i] && pos < (int)sizeof(msg) - 2; i++)
        msg[pos++] = REMOTE_DEFAULT_TOKEN[i];
    msg[pos++] = '\n';
    msg[pos] = '\0';

    if (net_tcp_connect_ip(NET_GW_HOST, REMOTE_DEFAULT_PORT, 10000) != 0) {
        kprintf("[remote] tcp connect failed agent=%d gw=10.0.2.2 port=%d\n",
                a ? a->id : -1, REMOTE_DEFAULT_PORT);
        return EIO;
    }
    if (net_tcp_write(msg, pos) < 0) {
        net_tcp_close();
        kprintf("[remote] tcp write failed agent=%d\n", a ? a->id : -1);
        return EIO;
    }

    n = net_tcp_read(resp, (int)sizeof(resp) - 1, 8000);
    net_tcp_close();
    if (n < 0 || !resp_has_pong(resp, n)) {
        kprintf("[remote] tcp connect bad response agent=%d n=%d\n",
                a ? a->id : -1, n);
        return EIO;
    }
    resp[n] = '\0';
    kprintf("[remote] tcp ok agent=%d port=%d resp=%s\n",
            a ? a->id : -1, REMOTE_DEFAULT_PORT, resp);
    return 0;
}
