#include "remote.h"
#include "printf.h"
#include "ramfs.h"

#define REMOTE_TOKEN_PATH "/sys/console/token"
#define REMOTE_DEFAULT_TOKEN "agentos-beta"

static int remote_enabled;

void remote_init(void) {
    remote_enabled = 0;
    ramfs_put(REMOTE_TOKEN_PATH, REMOTE_DEFAULT_TOKEN,
              (int)sizeof(REMOTE_DEFAULT_TOKEN) - 1);
    kprintf("[remote] ready port=%d token_path=%s (host proxy)\n",
            REMOTE_DEFAULT_PORT, REMOTE_TOKEN_PATH);
}

int remote_status(struct agent *a) {
    kprintf("[remote] agent=%d enabled=%d port=%d token=%s\n",
            a ? a->id : -1, remote_enabled, REMOTE_DEFAULT_PORT, REMOTE_DEFAULT_TOKEN);
    return remote_enabled;
}

int remote_enable(struct agent *a) {
    remote_enabled = 1;
    kprintf("[remote] enable agent=%d port=%d (use tools/remote-console.py)\n",
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
