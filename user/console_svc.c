#include "libagent.h"

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

static int starts_with(const char *s, const char *pfx) {
    int i = 0;
    while (pfx[i]) {
        if (s[i] != pfx[i])
            return 0;
        i++;
    }
    return 1;
}

static int console_read_char(void) {
    return (int)sys_agent_tool(TOOL_CONSOLE, CONSOLE_CMD_READ_CHAR, 0, 0);
}

static void console_puts(const char *s) {
    (void)sys_agent_tool(TOOL_CONSOLE, CONSOLE_CMD_PUTS, (long)s, 0);
}

static void console_prompt(void) {
    (void)sys_agent_tool(TOOL_CONSOLE, CONSOLE_CMD_PROMPT, 0, 0);
}

static int console_getline(char *buf, int len) {
    int pos = 0;
    int ch;
    char echo[2];

    if (len <= 0)
        return -1;

    for (;;) {
        ch = console_read_char();
        if (ch == 0) {
            agent_yield();
            continue;
        }
        if (ch < 0)
            return ch;
        if (ch == '\r' || ch == '\n') {
            buf[pos] = '\0';
            echo[0] = '\n';
            echo[1] = '\0';
            console_puts(echo);
            return pos;
        }
        if (ch == 127 || ch == 8) {
            if (pos > 0) {
                pos--;
                console_puts("\b \b");
            }
            continue;
        }
        if (pos >= len - 1)
            continue;
        buf[pos++] = (char)ch;
        echo[0] = (char)ch;
        echo[1] = '\0';
        console_puts(echo);
    }
}

static char *trim_line(char *s) {
    while (*s == ' ')
        s++;
    return s;
}

static void cmd_help(void) {
    console_puts("OpenAgentOS console commands:\n");
    console_puts("  /help              list commands\n");
    console_puts("  /echo <text>       display via display-agent\n");
    console_puts("  /llm <prompt>      ask router (faux backend)\n");
    console_puts("  /agents            list service agent ids\n");
    console_puts("  /session tail      print session tail\n");
    console_puts("  /session append T  append session line\n");
    console_puts("  /compact [keep]    compact session + summary\n");
    console_puts("  /steer <id> <msg>  send MSG_STEER\n");
    console_puts("  /run orch          start orchestrator pipeline\n");
    console_puts("  /status            print console agent phase\n");
    console_puts("  /list              list .agent packages\n");
    console_puts("  /load <path> [name] load agent ELF package\n");
    console_puts("  /ota verify <pkg>  verify OTA package\n");
    console_puts("  /ota apply <pkg>   apply OTA package\n");
    console_puts("  /catalog list      list signed package catalog\n");
    console_puts("  /catalog install <name>  install from catalog\n");
    console_puts("  /catalog rollback <name> rollback package\n");
    console_puts("  /tenant status     session quota usage\n");
    console_puts("  /tenant probe <id> cross-tenant read probe\n");
    console_puts("  /audit status      tool audit log usage\n");
    console_puts("  /audit tail        print audit tail\n");
    console_puts("  /audit compact [n] rotate audit log\n");
    console_puts("  /audit probe <id>  cross-agent audit probe\n");
    console_puts("  /namespace status  home mount usage\n");
    console_puts("  /namespace write T write home secret\n");
    console_puts("  /namespace probe <id> cross-namespace read probe\n");
    console_puts("  /quota status      ipc/tool/fs quota usage\n");
    console_puts("  /quota burn <n>    burn ipc send quota\n");
    console_puts("  /quota probe       probe ipc quota limit\n");
    console_puts("  /fleet status      fleet telemetry snapshot\n");
    console_puts("  /fleet push        export fleet.json cache\n");
    console_puts("  /fleet probe <url> probe collector endpoint\n");
    console_puts("  /fleet ingest [url] POST fleet.json to collector\n");
    console_puts("  /policy status     active deny rules\n");
    console_puts("  /policy load [path] load policy manifest\n");
    console_puts("  /policy deny <id>  deny tool by id\n");
    console_puts("  /policy allow <id> allow tool by id\n");
    console_puts("  /policy probe <id> probe tool policy\n");
    console_puts("  /remote status     remote console state\n");
    console_puts("  /remote enable     enable remote console\n");
    console_puts("  /remote disable    disable remote console\n");
    console_puts("  /remote ping       liveness check (requires enable)\n");
    console_puts("  /mesh status       mesh beacon state\n");
    console_puts("  /mesh beacon       emit UDP beacon (stub)\n");
    console_puts("  /mesh probe <svc>  probe local service\n");
    console_puts("  /quit              exit demo\n");
    console_puts("  /quit session      exit session demo\n");
    console_puts("  /quit orch         exit orch demo\n");
    console_puts("  /quit pack         exit pack demo\n");
    console_puts("  /quit catalog      exit catalog demo\n");
    console_puts("  /quit tenant       exit tenant demo\n");
    console_puts("  /quit audit        exit audit demo\n");
    console_puts("  /quit namespace    exit namespace demo\n");
    console_puts("  /quit quota        exit quota demo\n");
    console_puts("  /quit beta         exit v0.1-beta GA demo\n");
    console_puts("  /quit 0.3.0        exit 0.3.0 demo\n");
    console_puts("  /quit box          exit headless box demo\n");
}

static void console_boot_barrier(void) {
    int i;

    /* Let display/input finish boot logs before drawing the REPL prompt. */
    for (i = 0; i < 512; i++)
        agent_yield();
    console_puts("\n");
    console_prompt();
}

static void cmd_agents(void) {
    console_puts("services: id=6 router id=7 net id=8 display id=9 input id=10 console\n");
}

static int cmd_echo(const char *args) {
    args = trim_line((char *)args);
    if (!args[0]) {
        console_puts("usage: /echo <text>\n");
        return -1;
    }
    if (agent_svc_display(args, 8, 8) < 0) {
        console_puts("display failed\n");
        return -1;
    }
    return 0;
}

static int cmd_llm(const char *args) {
    char answer[LLM_MAX_RESPONSE];
    int n;

    args = trim_line((char *)args);
    if (!args[0]) {
        console_puts("usage: /llm <prompt>\n");
        return -1;
    }
    n = agent_svc_llm(args, answer, (int)sizeof(answer), "faux");
    if (n < 0) {
        console_puts("llm failed\n");
        return -1;
    }
    answer[n] = '\0';
    agent_log_str("answer: ");
    agent_log_str(answer);
    agent_log_str("\n");
    return 0;
}

static int parse_uint(const char *s, int *out) {
    int v = 0;
    int i = 0;

    while (s[i] == ' ')
        i++;
    if (s[i] < '0' || s[i] > '9')
        return 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    *out = v;
    return 1;
}

static int cmd_session(const char *args) {
    char buf[128];

    args = trim_line((char *)args);
    if (str_eq(args, "tail")) {
        int n = agent_session_tail(buf, (int)sizeof(buf));
        if (n < 0) {
            console_puts("session tail failed\n");
            return -1;
        }
        buf[n] = '\0';
        agent_log_str("[session] tail: ");
        agent_log_str(buf);
        agent_log_str("\n");
        return 0;
    }
    if (starts_with(args, "append ")) {
        const char *text = trim_line((char *)(args + 7));
        if (!text[0]) {
            console_puts("usage: /session append <text>\n");
            return -1;
        }
        if (agent_session_append(text) < 0) {
            console_puts("session append failed\n");
            return -1;
        }
        return 0;
    }
    console_puts("usage: /session tail|append <text>\n");
    return -1;
}

static int cmd_compact(const char *args) {
    char buf[128];
    const char *body;
    int keep = 2;
    int rc;
    int i;

    args = trim_line((char *)args);
    if (args[0] && !parse_uint(args, &keep)) {
        console_puts("usage: /compact [keep]\n");
        return -1;
    }
    rc = agent_session_compact(keep);
    if (rc < 0) {
        console_puts("compact failed\n");
        return -1;
    }
    if (rc == 0) {
        agent_log_str("[session] compact: nothing to drop\n");
        return 0;
    }
    rc = agent_session_read(buf, (int)sizeof(buf), 0);
    if (rc < 0) {
        console_puts("compact read failed\n");
        return -1;
    }
    for (i = 0; i < rc && buf[i] != '\n'; i++)
        ;
    buf[i] = '\0';
    body = buf;
    if (starts_with(buf, "summary:")) {
        body = trim_line((char *)(buf + 8));
    }
    agent_log_str("[session] summary: ");
    agent_log_str(body);
    agent_log_str("\n");
    return 0;
}

static int cmd_steer(const char *args) {
    int dst = 0;
    const char *msg;

    args = trim_line((char *)args);
    if (!parse_uint(args, &dst)) {
        console_puts("usage: /steer <agent-id> <msg>\n");
        return -1;
    }
    while (*args == ' ')
        args++;
    while (*args >= '0' && *args <= '9')
        args++;
    msg = trim_line((char *)args);
    if (!msg[0]) {
        console_puts("usage: /steer <agent-id> <msg>\n");
        return -1;
    }
    if (agent_send_steering(dst, msg) < 0) {
        console_puts("steer failed\n");
        return -1;
    }
    agent_log_str("[console] steer sent to ");
    agent_log_int(dst);
    agent_log_str("\n");
    return 0;
}

static int cmd_run(const char *args) {
    args = trim_line((char *)args);
    if (!str_eq(args, "orch")) {
        console_puts("usage: /run orch\n");
        return -1;
    }
    if (agent_send_msg(1, MSG_CONSOLE_REQ, "run:orch") < 0) {
        console_puts("orch dispatch failed\n");
        return -1;
    }
    agent_log_str("[console] orch dispatch sent\n");
    return 0;
}

static void cmd_status(void) {
    agent_log_str("[status] console phase=");
    agent_log_int(agent_phase());
    agent_log_str(" self=");
    agent_log_int(agent_self());
    agent_log_str("\n");
}

static int pack_dispatch(const char *payload) {
    if (agent_send_msg(1, MSG_CONSOLE_REQ, payload) < 0) {
        console_puts("pack request failed\n");
        return -1;
    }
    agent_yield();
    return 0;
}

static int cmd_list(void) {
    agent_log_str("[console] list packages\n");
    return pack_dispatch("list");
}

static int cmd_load(const char *args) {
    char payload[MSG_PAYLOAD_SIZE];
    const char *path;
    const char *name;
    char *walk;
    char *split;
    int pos = 0;
    int i;

    walk = trim_line((char *)args);
    if (!walk[0]) {
        console_puts("usage: /load <path> [name]\n");
        return -1;
    }
    split = walk;
    while (*split) {
        if (*split == ' ') {
            *split++ = '\0';
            break;
        }
        split++;
    }
    path = walk;
    name = trim_line(split);
    if (!name[0])
        name = "worker";

    payload[pos++] = 'l';
    payload[pos++] = 'o';
    payload[pos++] = 'a';
    payload[pos++] = 'd';
    payload[pos++] = ':';
    for (i = 0; path[i] && pos < (int)sizeof(payload) - 2; i++)
        payload[pos++] = path[i];
    if (pos >= (int)sizeof(payload) - 2)
        return -1;
    payload[pos++] = '|';
    for (i = 0; name[i] && pos < (int)sizeof(payload) - 1; i++)
        payload[pos++] = name[i];
    payload[pos] = '\0';

    agent_log_str("[console] load ");
    agent_log_str(path);
    agent_log_str("\n");
    return pack_dispatch(payload);
}

static int cmd_ota(const char *args) {
    char payload[MSG_PAYLOAD_SIZE];
    const char *path;
    int pos = 0;
    int i;

    args = trim_line((char *)args);
    if (starts_with(args, "verify ")) {
        path = trim_line((char *)(args + 7));
        if (!path[0]) {
            console_puts("usage: /ota verify <pkg>\n");
            return -1;
        }
        for (i = 0; "ota-verify:"[i]; i++)
            payload[pos++] = "ota-verify:"[i];
        for (i = 0; path[i] && pos < (int)sizeof(payload) - 1; i++)
            payload[pos++] = path[i];
        payload[pos] = '\0';
        return pack_dispatch(payload);
    }
    if (starts_with(args, "apply ")) {
        path = trim_line((char *)(args + 6));
        if (!path[0]) {
            console_puts("usage: /ota apply <pkg>\n");
            return -1;
        }
        for (i = 0; "ota-apply:"[i]; i++)
            payload[pos++] = "ota-apply:"[i];
        for (i = 0; path[i] && pos < (int)sizeof(payload) - 1; i++)
            payload[pos++] = path[i];
        payload[pos] = '\0';
        return pack_dispatch(payload);
    }
    console_puts("usage: /ota verify|apply <pkg>\n");
    return -1;
}

static int cmd_catalog(const char *args) {
    char payload[MSG_PAYLOAD_SIZE];
    const char *name;
    int pos = 0;
    int i;

    args = trim_line((char *)args);
    if (str_eq(args, "list") || !args[0]) {
        return pack_dispatch("catalog-list");
    }
    if (starts_with(args, "install ")) {
        name = trim_line((char *)(args + 8));
        if (!name[0]) {
            console_puts("usage: /catalog install <name>\n");
            return -1;
        }
        for (i = 0; "catalog-install:"[i]; i++)
            payload[pos++] = "catalog-install:"[i];
        for (i = 0; name[i] && pos < (int)sizeof(payload) - 1; i++)
            payload[pos++] = name[i];
        payload[pos] = '\0';
        return pack_dispatch(payload);
    }
    if (starts_with(args, "rollback ")) {
        name = trim_line((char *)(args + 9));
        if (!name[0]) {
            console_puts("usage: /catalog rollback <name>\n");
            return -1;
        }
        for (i = 0; "catalog-rollback:"[i]; i++)
            payload[pos++] = "catalog-rollback:"[i];
        for (i = 0; name[i] && pos < (int)sizeof(payload) - 1; i++)
            payload[pos++] = name[i];
        payload[pos] = '\0';
        return pack_dispatch(payload);
    }
    console_puts("usage: /catalog list|install <name>|rollback <name>\n");
    return -1;
}

static int cmd_tenant(const char *args) {
    int target = 1;
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_TENANT, TENANT_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("tenant status failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        const char *walk = trim_line((char *)(args + 5));
        if (walk[0] >= '0' && walk[0] <= '9') {
            target = 0;
            while (*walk >= '0' && *walk <= '9') {
                target = target * 10 + (*walk - '0');
                walk++;
            }
        }
        rc = (int)sys_agent_tool(TOOL_TENANT, TENANT_CMD_PROBE, target, 0);
        if (rc == EPERM) {
            agent_log_str("[tenant] isolation ok\n");
            return 0;
        }
        if (rc < 0)
            return 0;
        console_puts("tenant probe unexpected allow\n");
        return -1;
    }
    console_puts("usage: /tenant status|probe <agent-id>\n");
    return -1;
}

static int cmd_audit(const char *args) {
    int target = 1;
    int keep = 4;
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_AUDIT, AUDIT_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("audit status failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "tail")) {
        rc = (int)sys_agent_tool(TOOL_AUDIT, AUDIT_CMD_TAIL, AUDIT_TAIL_MAX, 0);
        if (rc < 0) {
            console_puts("audit tail failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "compact")) {
        const char *walk = trim_line((char *)(args + 7));
        if (walk[0] >= '0' && walk[0] <= '9') {
            keep = 0;
            while (*walk >= '0' && *walk <= '9') {
                keep = keep * 10 + (*walk - '0');
                walk++;
            }
        }
        if (keep < 1)
            keep = 4;
        rc = (int)sys_agent_tool(TOOL_AUDIT, AUDIT_CMD_COMPACT, keep, 0);
        if (rc < 0) {
            console_puts("audit compact failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        const char *walk = trim_line((char *)(args + 5));
        if (walk[0] >= '0' && walk[0] <= '9') {
            target = 0;
            while (*walk >= '0' && *walk <= '9') {
                target = target * 10 + (*walk - '0');
                walk++;
            }
        }
        rc = (int)sys_agent_tool(TOOL_AUDIT, AUDIT_CMD_PROBE, target, 0);
        if (rc == EPERM) {
            agent_log_str("[audit] isolation ok\n");
            return 0;
        }
        if (rc < 0)
            return 0;
        console_puts("audit probe unexpected allow\n");
        return -1;
    }
    console_puts("usage: /audit status|tail|compact [n]|probe <agent-id>\n");
    return -1;
}

static int cmd_namespace(const char *args) {
    int target = 1;
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_NAMESPACE, NS_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("namespace status failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "write")) {
        const char *walk = trim_line((char *)(args + 5));
        if (!walk[0]) {
            console_puts("usage: /namespace write <text>\n");
            return -1;
        }
        rc = (int)sys_agent_tool(TOOL_NAMESPACE, NS_CMD_WRITE, (long)walk, 0);
        if (rc < 0) {
            console_puts("namespace write failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        const char *walk = trim_line((char *)(args + 5));
        if (walk[0] >= '0' && walk[0] <= '9') {
            target = 0;
            while (*walk >= '0' && *walk <= '9') {
                target = target * 10 + (*walk - '0');
                walk++;
            }
        }
        rc = (int)sys_agent_tool(TOOL_NAMESPACE, NS_CMD_PROBE, target, 0);
        if (rc == EPERM) {
            agent_log_str("[namespace] isolation ok\n");
            return 0;
        }
        if (rc < 0)
            return 0;
        console_puts("namespace probe unexpected allow\n");
        return -1;
    }
    console_puts("usage: /namespace status|write <text>|probe <agent-id>\n");
    return -1;
}

static int cmd_quota(const char *args) {
    int count = 16;
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("quota status failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "burn")) {
        const char *walk = trim_line((char *)(args + 4));
        if (walk[0] >= '0' && walk[0] <= '9') {
            count = 0;
            while (*walk >= '0' && *walk <= '9') {
                count = count * 10 + (*walk - '0');
                walk++;
            }
        }
        if (count < 1)
            count = 1;
        rc = (int)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_BURN, count, 0);
        if (rc < 0) {
            console_puts("quota burn failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "probe")) {
        rc = (int)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_PROBE, 0, 0);
        if (rc == ENOSPC) {
            agent_log_str("[quota] limit ok\n");
            return 0;
        }
        if (rc < 0)
            return 0;
        console_puts("quota probe unexpected allow\n");
        return -1;
    }
    console_puts("usage: /quota status|burn <n>|probe\n");
    return -1;
}

static int parse_tool_id(const char *s, int *out) {
    int id = 0;

    s = trim_line((char *)s);
    if (*s < '0' || *s > '9')
        return -1;
    while (*s >= '0' && *s <= '9') {
        id = id * 10 + (*s - '0');
        s++;
    }
    if (*s != '\0')
        return -1;
    *out = id;
    return 0;
}

static int cmd_fleet(const char *args) {
    char url[128];
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_FLEET, FLEET_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("fleet status failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "push")) {
        rc = (int)sys_agent_tool(TOOL_FLEET, FLEET_CMD_PUSH, 0, 0);
        if (rc < 0) {
            console_puts("fleet push failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        const char *walk = trim_line((char *)(args + 5));
        if (!walk[0]) {
            console_puts("usage: /fleet probe <url>\n");
            return -1;
        }
        {
            int i = 0;
            while (walk[i] && i < (int)sizeof(url) - 1) {
                url[i] = walk[i];
                i++;
            }
            url[i] = '\0';
        }
        rc = (int)sys_agent_tool(TOOL_FLEET, FLEET_CMD_PROBE, (long)url, 0);
        if (rc < 0) {
            console_puts("fleet probe failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "ingest")) {
        const char *walk = trim_line((char *)(args + 6));
        if (!walk[0]) {
            rc = (int)sys_agent_tool(TOOL_FLEET, FLEET_CMD_INGEST, 0, 0);
        } else {
            int i = 0;
            while (walk[i] && i < (int)sizeof(url) - 1) {
                url[i] = walk[i];
                i++;
            }
            url[i] = '\0';
            rc = (int)sys_agent_tool(TOOL_FLEET, FLEET_CMD_INGEST, (long)url, 0);
        }
        if (rc < 0) {
            console_puts("fleet ingest failed\n");
            return -1;
        }
        return 0;
    }
    console_puts("usage: /fleet status|push|probe <url>|ingest [url]\n");
    return -1;
}

static int cmd_policy(const char *args) {
    char path[128];
    int tool_id;
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("policy status failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "load")) {
        const char *walk = trim_line((char *)(args + 4));
        if (!walk[0])
            rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_LOAD, 0, 0);
        else {
            int i = 0;
            while (walk[i] && i < (int)sizeof(path) - 1) {
                path[i] = walk[i];
                i++;
            }
            path[i] = '\0';
            rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_LOAD, (long)path, 0);
        }
        if (rc < 0) {
            console_puts("policy load failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "deny")) {
        if (parse_tool_id(args + 4, &tool_id) < 0) {
            console_puts("usage: /policy deny <tool-id>\n");
            return -1;
        }
        rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_DENY, tool_id, 0);
        if (rc < 0) {
            console_puts("policy deny failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "allow")) {
        if (parse_tool_id(args + 5, &tool_id) < 0) {
            console_puts("usage: /policy allow <tool-id>\n");
            return -1;
        }
        rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_ALLOW, tool_id, 0);
        if (rc < 0) {
            console_puts("policy allow failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        if (parse_tool_id(args + 5, &tool_id) < 0) {
            console_puts("usage: /policy probe <tool-id>\n");
            return -1;
        }
        rc = (int)sys_agent_tool(TOOL_POLICY, POLICY_CMD_PROBE, tool_id, 0);
        if (rc == EPERM) {
            agent_log_str("[policy] deny ok\n");
            return 0;
        }
        if (rc < 0)
            return 0;
        agent_log_str("[policy] allow ok\n");
        return 0;
    }
    console_puts("usage: /policy status|load [path]|deny|allow|probe <tool-id>\n");
    return -1;
}

static int cmd_remote(const char *args) {
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_REMOTE, REMOTE_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("remote status failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "enable")) {
        rc = (int)sys_agent_tool(TOOL_REMOTE, REMOTE_CMD_ENABLE, 0, 0);
        if (rc < 0) {
            console_puts("remote enable failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "disable")) {
        rc = (int)sys_agent_tool(TOOL_REMOTE, REMOTE_CMD_DISABLE, 0, 0);
        if (rc < 0) {
            console_puts("remote disable failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "ping")) {
        rc = (int)sys_agent_tool(TOOL_REMOTE, REMOTE_CMD_PING, 0, 0);
        if (rc < 0) {
            console_puts("remote ping failed\n");
            return -1;
        }
        return 0;
    }
    console_puts("usage: /remote status|enable|disable|ping\n");
    return -1;
}

static int cmd_mesh(const char *args) {
    char service[64];
    int rc;

    args = trim_line((char *)args);
    if (str_eq(args, "status") || !args[0]) {
        rc = (int)sys_agent_tool(TOOL_MESH, MESH_CMD_STATUS, 0, 0);
        if (rc < 0) {
            console_puts("mesh status failed\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args, "beacon")) {
        rc = (int)sys_agent_tool(TOOL_MESH, MESH_CMD_BEACON, 0, 0);
        if (rc < 0) {
            console_puts("mesh beacon failed\n");
            return -1;
        }
        return 0;
    }
    if (starts_with(args, "probe")) {
        const char *walk = trim_line((char *)(args + 5));
        if (!walk[0]) {
            console_puts("usage: /mesh probe <service>\n");
            return -1;
        }
        {
            int i = 0;
            while (walk[i] && i < (int)sizeof(service) - 1) {
                service[i] = walk[i];
                i++;
            }
            service[i] = '\0';
        }
        rc = (int)sys_agent_tool(TOOL_MESH, MESH_CMD_PROBE, (long)service, 0);
        if (rc < 0) {
            console_puts("mesh probe failed\n");
            return -1;
        }
        return 0;
    }
    console_puts("usage: /mesh status|beacon|probe <service>\n");
    return -1;
}

static int handle_line(char *line) {
    const char *args;

    line = trim_line(line);
    if (!line[0])
        return 0;

    if (str_eq(line, "/help") || starts_with(line, "/help ")) {
        cmd_help();
        return 0;
    }
    if (str_eq(line, "/agents")) {
        cmd_agents();
        return 0;
    }
    if (str_eq(line, "/quit")) {
        agent_log_str("[console] quit\n");
        agent_send_msg(1, MSG_RESULT, "console demo complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit session")) {
        agent_log_str("[console] quit session\n");
        agent_send_msg(1, MSG_RESULT, "console demo session complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit orch")) {
        agent_log_str("[console] quit orch\n");
        agent_send_msg(1, MSG_RESULT, "console demo orch complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit pack")) {
        agent_log_str("[console] quit pack\n");
        agent_send_msg(1, MSG_RESULT, "console demo pack complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit catalog")) {
        agent_log_str("[console] quit catalog\n");
        agent_send_msg(1, MSG_RESULT, "catalog demo complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit tenant")) {
        agent_log_str("[console] quit tenant\n");
        agent_send_msg(1, MSG_RESULT, "console demo tenant complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit audit")) {
        agent_log_str("[console] quit audit\n");
        agent_send_msg(1, MSG_RESULT, "console demo audit complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit namespace")) {
        agent_log_str("[console] quit namespace\n");
        agent_send_msg(1, MSG_RESULT, "console demo namespace complete");
        agent_exit(0);
    }
    if (str_eq(line, "/quit quota")) {
        agent_log_str("[console] quit quota\n");
        (void)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_NOTIFY,
                             (long)"console demo quota complete", 0);
        agent_exit(0);
    }
    if (str_eq(line, "/quit beta")) {
        agent_log_str("[console] quit beta\n");
        (void)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_NOTIFY,
                             (long)"v0.1-beta demo complete", 0);
        agent_exit(0);
    }
    if (str_eq(line, "/quit v7")) {
        agent_log_str("[console] quit v7\n");
        (void)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_NOTIFY,
                             (long)"console demo v7 complete", 0);
        agent_exit(0);
    }
    if (str_eq(line, "/quit rc")) {
        agent_log_str("[console] quit rc\n");
        (void)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_NOTIFY,
                             (long)"v0.2-rc demo complete", 0);
        agent_exit(0);
    }
    if (str_eq(line, "/quit 0.3.0")) {
        agent_log_str("[console] quit 0.3.0\n");
        (void)sys_agent_tool(TOOL_QUOTA, QUOTA_CMD_NOTIFY,
                             (long)"0.3.0 demo complete", 0);
        agent_exit(0);
    }
    if (str_eq(line, "/quit box")) {
        agent_log_str("[console] quit box\n");
        agent_send_msg(1, MSG_RESULT, "box demo complete");
        agent_exit(0);
    }
    if (str_eq(line, "/list")) {
        return cmd_list();
    }
    if (str_eq(line, "/status")) {
        cmd_status();
        return 0;
    }
    if (starts_with(line, "/run")) {
        args = line + 4;
        return cmd_run(args);
    }
    if (starts_with(line, "/load")) {
        args = line + 5;
        return cmd_load(args);
    }
    if (starts_with(line, "/ota")) {
        args = line + 4;
        return cmd_ota(args);
    }
    if (starts_with(line, "/catalog")) {
        args = line + 8;
        return cmd_catalog(args);
    }
    if (starts_with(line, "/tenant")) {
        args = line + 7;
        return cmd_tenant(args);
    }
    if (starts_with(line, "/audit")) {
        args = line + 6;
        return cmd_audit(args);
    }
    if (starts_with(line, "/namespace")) {
        args = line + 10;
        return cmd_namespace(args);
    }
    if (starts_with(line, "/quota")) {
        args = line + 6;
        return cmd_quota(args);
    }
    if (starts_with(line, "/fleet")) {
        args = line + 6;
        return cmd_fleet(args);
    }
    if (starts_with(line, "/policy")) {
        args = line + 7;
        return cmd_policy(args);
    }
    if (starts_with(line, "/remote")) {
        args = line + 7;
        return cmd_remote(args);
    }
    if (starts_with(line, "/mesh")) {
        args = line + 5;
        return cmd_mesh(args);
    }
    if (starts_with(line, "/session")) {
        args = line + 8;
        return cmd_session(args);
    }
    if (starts_with(line, "/compact")) {
        args = line + 8;
        return cmd_compact(args);
    }
    if (starts_with(line, "/steer")) {
        args = line + 6;
        return cmd_steer(args);
    }
    if (starts_with(line, "/echo")) {
        args = line + 5;
        return cmd_echo(args);
    }
    if (starts_with(line, "/llm")) {
        args = line + 4;
        return cmd_llm(args);
    }

    console_puts("unknown command; try /help\n");
    return -1;
}

void console_agent_main(void) {
    char line[CONSOLE_MAX_LINE];
    int n;

    agent_log_str("[console] service ready id=");
    agent_log_int(CONSOLE_AGENT_ID);
    agent_log_str("\n");

    console_boot_barrier();
    for (;;) {
        n = console_getline(line, (int)sizeof(line));
        if (n < 0)
            continue;
        handle_line(line);
        console_prompt();
    }
}
