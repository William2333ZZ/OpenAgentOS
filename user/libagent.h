#ifndef LIBAGENT_H
#define LIBAGENT_H

#include "../include/agentos.h"

#define MSG_TASK   1
#define MSG_RESULT 2

static inline void agent_yield(void) {
    sys_agent_yield();
}

static inline int agent_self(void) {
    return (int)sys_agent_self();
}

static inline int agent_spawn(void (*entry)(void), const char *name, uint32_t caps) {
    return (int)sys_agent_create(entry, name, caps);
}

static inline int agent_load(const char *path, const char *name, uint32_t caps) {
    return (int)sys_agent_load(path, name, caps);
}

static inline int agent_send_msg(int dst, int type, const char *payload) {
    return (int)sys_agent_send(dst, type, payload);
}

static inline int agent_send_steering(int dst, const char *payload) {
    return (int)sys_agent_send(dst, MSG_STEER, payload);
}

static inline int agent_send_followup(int dst, const char *payload) {
    return (int)sys_agent_send(dst, MSG_FOLLOWUP, payload);
}

static inline int agent_try_recv(struct agent_msg *msg) {
    return (int)sys_agent_recv(msg);
}

static inline int agent_recv_msg(struct agent_msg *msg) {
    long rc;
    while ((rc = sys_agent_recv(msg)) == EAGAIN)
        agent_yield();
    return (int)rc;
}

static inline int agent_compact_inbox(int keep) {
    return (int)sys_agent_compact(keep);
}

static inline int agent_phase(void) {
    return (int)sys_agent_get_phase();
}

static inline int str_len(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static inline void agent_log_str(const char *s) {
    sys_agent_write(s, str_len(s));
}

static inline void agent_log_int(int val) {
    char buf[16];
    int i = 0;
    unsigned int n;
    if (val < 0) {
        agent_log_str("-");
        n = (unsigned int)(-val);
    } else {
        n = (unsigned int)val;
    }
    if (n == 0) {
        agent_log_str("0");
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        char c[2] = { buf[--i], '\0' };
        agent_log_str(c);
    }
}

static inline int agent_add(int a, int b) {
    return (int)sys_agent_tool(TOOL_ADD, a, b, 0);
}

static inline int agent_time(long *out_ticks) {
    return (int)sys_agent_tool(TOOL_TIME, (long)out_ticks, 0, 0);
}

static inline int agent_read_file(const char *path, char *buf, int len) {
    return (int)sys_agent_tool(TOOL_READ, (long)path, (long)buf, len);
}

static inline int agent_write_file(const char *path, const char *data, int len) {
    return (int)sys_agent_tool(TOOL_WRITE, (long)path, (long)data, len);
}

static inline int agent_tool_llm(const char *prompt, char *buf, int buflen) {
    return (int)sys_agent_tool(TOOL_LLM, (long)prompt, (long)buf, buflen);
}

static inline int agent_session_append(const char *text) {
    return (int)sys_agent_tool(TOOL_SESSION_APPEND, (long)text, 0, 0);
}

static inline int agent_session_tail(char *buf, int buflen) {
    return (int)sys_agent_tool(TOOL_SESSION_TAIL, (long)buf, buflen, 0);
}

static inline int agent_session_read(char *buf, int buflen, int max_lines) {
    return (int)sys_agent_tool(TOOL_SESSION_READ, (long)buf, buflen, max_lines);
}

static inline int agent_session_compact(int keep) {
    return (int)sys_agent_tool(TOOL_SESSION_COMPACT, keep, 0, 0);
}

static inline int agent_gpio(int pin, int val) {
    return (int)sys_agent_tool(TOOL_GPIO, pin, val, 0);
}

static inline int agent_http(const char *url, char *buf, int len) {
    return (int)sys_agent_tool(TOOL_HTTP, (long)url, (long)buf, len);
}

static inline int agent_session_compact_recv(int keep, char *summary, int sumlen) {
    struct agent_msg msg;
    int rc;
    int i;

    rc = agent_session_compact(keep);
    if (rc < 0)
        return rc;
    if (rc == 0)
        return 0;
    agent_recv_msg(&msg);
    if (msg.type != MSG_SUMMARY)
        return -1;
    if (summary && sumlen > 0) {
        for (i = 0; msg.payload[i] && i < sumlen - 1; i++)
            summary[i] = msg.payload[i];
        summary[i] = '\0';
    }
    return rc;
}

static inline int agent_sync(void) {
    return (int)sys_agent_sync();
}

int agent_svc_read(const char *path, char *buf, int len);
int agent_svc_write(const char *path, const char *data, int len);
int agent_svc_llm(const char *prompt, char *buf, int len, const char *backend);
int agent_svc_http(const char *url, char *buf, int len, const char *backend);
int agent_svc_display(const char *text, int x, int y);
int agent_svc_input_poll(void);

static inline int svc_append_int(char *buf, int pos, int maxlen, int val) {
    char tmp[12];
    int i = 0;
    unsigned int n;

    if (val < 0)
        return -1;
    n = (unsigned int)val;
    if (n == 0) {
        if (pos >= maxlen - 1)
            return -1;
        buf[pos++] = '0';
        return pos;
    }
    while (n > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        if (pos >= maxlen - 1)
            return -1;
        buf[pos++] = tmp[--i];
    }
    return pos;
}

static inline int svc_parse_rsp_id(const char *payload, int expect_id,
                                   const char **body) {
    int id = 0;
    int i = 1;

    *body = payload;
    if (payload[0] != 'R' && payload[0] != 'W')
        return expect_id == 0;
    if (payload[1] < '0' || payload[1] > '9')
        return 0;
    while (payload[i] >= '0' && payload[i] <= '9') {
        id = id * 10 + (payload[i] - '0');
        i++;
    }
    if (payload[i] != '|')
        return 0;
    *body = payload + i + 1;
    return id == expect_id;
}

static inline int agent_llm(const char *prompt, char *buf, int buflen) {
    return (int)sys_agent_llm(prompt, buf, buflen);
}

static inline void agent_exit(int code) {
    sys_agent_exit(code);
}

#endif
