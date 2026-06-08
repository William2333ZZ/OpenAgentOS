#include "../include/agentos.h"

#define MSG_RESULT 2
#define INIT_AGENT_ID 1

static int str_len(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void log_str(const char *s) {
    sys_agent_write(s, str_len(s));
}

static void log_int(int val) {
    char buf[16];
    int i = 0;
    unsigned int n;

    if (val < 0) {
        log_str("-");
        n = (unsigned int)(-val);
    } else {
        n = (unsigned int)val;
    }
    if (n == 0) {
        log_str("0");
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        char c[2] = { buf[--i], '\0' };
        log_str(c);
    }
}

static int tool_add(int a, int b) {
    return (int)sys_agent_tool(TOOL_ADD, a, b, 0);
}

static char g_result[32];

void worker_main(void) {
    int sum;
    int pos = 0;
    const char *prefix = "sum=";

    log_str("[worker] loaded agent running\n");
    sum = tool_add(17, 25);
    log_str("[worker] sum=");
    log_int(sum);
    log_str("\n");

    while (prefix[pos]) {
        g_result[pos] = prefix[pos];
        pos++;
    }
    {
        char tmp[12];
        int i = 0;
        unsigned int n = (unsigned int)sum;
        if (n == 0)
            tmp[i++] = '0';
        while (n > 0) {
            tmp[i++] = '0' + (n % 10);
            n /= 10;
        }
        while (i > 0)
            g_result[pos++] = tmp[--i];
    }
    g_result[pos] = '\0';

    if (sum != 42) {
        log_str("[worker] bad sum\n");
        sys_agent_exit(1);
    }

    sys_agent_send(INIT_AGENT_ID, MSG_RESULT, g_result);
    log_str("[worker] done\n");
    sys_agent_exit(0);
}
