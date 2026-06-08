#ifndef AGENTOS_H
#define AGENTOS_H

#include <stdint.h>

#define MAX_AGENTS       12
#define AGENT_STACK_SIZE 8192
#ifdef PLATFORM_X86_64_PC
#define USER_STACK_BASE  0x40000000UL
#define USER_HEAP_BASE   0x60000000UL
#else
#define USER_STACK_BASE  0x00000040000000UL
#define USER_HEAP_BASE   0x00000060000000UL
#endif
#define USER_STACK_SLOT  (AGENT_STACK_SIZE + 4096)
#define MSG_CAPACITY     16
#define MSG_STEER_CAPACITY 4
#define MSG_PAYLOAD_SIZE 224

/* User-defined message types */
#define MSG_USER_MIN     1
#define MSG_USER_MAX     0x7fff
#define MSG_RESULT       2

/* Agent protocol (pi-inspired: steer / follow-up / compaction) */
#define MSG_STEER        0x8001
#define MSG_FOLLOWUP     0x8002
#define MSG_SUMMARY      0x8003
#define MSG_PIPELINE_DONE 0x8004
#define MSG_FS_READ       0x8010
#define MSG_FS_WRITE      0x8011
#define MSG_FS_RSP        0x8012
#define MSG_LLM_REQ       0x8020
#define MSG_LLM_RSP       0x8021
#define MSG_HTTP_REQ      0x8022
#define MSG_HTTP_RSP      0x8023
#define MSG_DISPLAY_REQ   0x8024
#define MSG_DISPLAY_RSP   0x8025
#define MSG_INPUT_REQ     0x8026
#define MSG_INPUT_RSP     0x8027
#define MSG_CONSOLE_REQ   0x8028
#define MSG_CONSOLE_RSP   0x8029
#define MSG_SHELL_REQ     0x802A
#define MSG_SHELL_RSP     0x802B

#define CAP_LOG   (1u << 0)
#define CAP_MATH  (1u << 1)
#define CAP_SPAWN (1u << 2)
#define CAP_SEND  (1u << 3)
#define CAP_RECV  (1u << 4)
#define CAP_LLM   (1u << 5)
#define CAP_FS    (1u << 6)
#define CAP_TIME  (1u << 7)
#define CAP_SVC_STORAGE (1u << 8)
#define CAP_NET   (1u << 9)
#define CAP_GPIO  (1u << 10)
#define CAP_SVC_LLM (1u << 11)
#define CAP_SVC_NET (1u << 12)
#define CAP_DISPLAY (1u << 13)
#define CAP_INPUT   (1u << 14)
#define CAP_SVC_DISPLAY (1u << 15)
#define CAP_SVC_INPUT   (1u << 16)
#define CAP_SVC_CONSOLE (1u << 17)
#define CAP_SVC_SHELL   (1u << 18)

#define EPERM      (-1)
#define EINVAL     (-2)
#define ENOENT     (-3)
#define ENOSPC     (-4)
#define EAGAIN     (-5)
#define ENODEV     (-6)
#define EIO        (-7)
#define ETIMEDOUT  (-8)
#define EFAULT     (-9)

#define SYS_AGENT_CREATE   1
#define SYS_AGENT_DESTROY  2
#define SYS_AGENT_SEND     3
#define SYS_AGENT_RECV     4
#define SYS_AGENT_YIELD    5
#define SYS_AGENT_SELF     6
#define SYS_AGENT_WRITE    7
#define SYS_AGENT_TOOL     8
#define SYS_AGENT_EXIT     9
#define SYS_AGENT_LLM      10
#define SYS_AGENT_COMPACT  11
#define SYS_AGENT_GET_PHASE 12
#define SYS_AGENT_SYNC      13
#define SYS_AGENT_HEAP_BASE 14
#define SYS_AGENT_HEAP_MAP  15
#define SYS_AGENT_HEAP_COW  16
#define SYS_AGENT_LOAD      17

#define LLM_MAX_PROMPT   256
#define LLM_MAX_RESPONSE 512
#define HTTP_MAX_BODY    512

#define TOOL_LOG 0
#define TOOL_ADD 1
#define TOOL_TIME 2
#define TOOL_READ 3
#define TOOL_WRITE 4
#define TOOL_LLM 5
#define TOOL_SESSION_APPEND 6
#define TOOL_SESSION_TAIL   7
#define TOOL_SESSION_READ   8
#define TOOL_SESSION_COMPACT 9
#define TOOL_GPIO 10
#define TOOL_HTTP 11
#define TOOL_SMP  12
#define TOOL_OTA  13
#define TOOL_DISPLAY 14
#define TOOL_INPUT   15
#define TOOL_CONSOLE 16
#define TOOL_CATALOG 17
#define TOOL_TENANT  18
#define TOOL_AUDIT   19
#define TOOL_NAMESPACE 20
#define TOOL_QUOTA     21
#define TOOL_FLEET     22
#define TOOL_POLICY    23
#define TOOL_REMOTE    24
#define TOOL_MESH      25

#define FLEET_CMD_STATUS 0
#define FLEET_CMD_PUSH   1
#define FLEET_CMD_PROBE  2
#define FLEET_CMD_INGEST 3

#define POLICY_CMD_STATUS 0
#define POLICY_CMD_LOAD   1
#define POLICY_CMD_PROBE  2
#define POLICY_CMD_DENY   3
#define POLICY_CMD_ALLOW  4

#define REMOTE_CMD_STATUS  0
#define REMOTE_CMD_ENABLE  1
#define REMOTE_CMD_DISABLE 2
#define REMOTE_CMD_PING    3
#define REMOTE_CMD_CONNECT 4

#define MESH_CMD_STATUS 0
#define MESH_CMD_BEACON 1
#define MESH_CMD_PROBE  2

#define CONSOLE_MAX_LINE 200

#define CONSOLE_CMD_GETLINE 0
#define CONSOLE_CMD_PUTS    1
#define CONSOLE_CMD_PROMPT  2
#define CONSOLE_CMD_READ_CHAR 3

#define OTA_CMD_APPLY        0
#define OTA_CMD_VERIFY       1
#define OTA_CMD_CHANNEL_GET  2
#define OTA_CMD_CHANNEL_SET  3
#define OTA_CMD_KERNEL_GEN   4

#define CATALOG_CMD_LIST     0
#define CATALOG_CMD_INSTALL  1
#define CATALOG_CMD_ROLLBACK 2

#define TENANT_CMD_STATUS 0
#define TENANT_CMD_PROBE  1

#define AUDIT_CMD_STATUS  0
#define AUDIT_CMD_TAIL    1
#define AUDIT_CMD_PROBE   2
#define AUDIT_CMD_COMPACT 3
#define AUDIT_TAIL_MAX    128

#define NS_CMD_STATUS 0
#define NS_CMD_PROBE  1
#define NS_CMD_WRITE  2

#define QUOTA_CMD_STATUS 0
#define QUOTA_CMD_BURN   1
#define QUOTA_CMD_PROBE  2
#define QUOTA_CMD_NOTIFY 3

#define STORAGE_AGENT_ID 2
#define ROUTER_AGENT_ID  6
#define NET_AGENT_ID     7
#define DISPLAY_AGENT_ID 8
#define INPUT_AGENT_ID   9
#define CONSOLE_AGENT_ID 10
#define SHELL_AGENT_ID   11

enum agent_phase {
    AGENT_PHASE_IDLE = 0,
    AGENT_PHASE_TURN,
    AGENT_PHASE_TOOL,
    AGENT_PHASE_LLM_WAIT,
    AGENT_PHASE_WAIT_IPC,
};

struct agent_msg {
    int sender_id;
    int type;
    char payload[MSG_PAYLOAD_SIZE];
};

long kernel_dispatch_syscall(long n, long a0, long a1, long a2,
                             long a3, long a4, long a5);

#ifdef PLATFORM_X86_64_PC
static inline long syscall6(long n, long a0, long a1, long a2,
                            long a3, long a4, long a5) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(n), "b"(a0), "c"(a1), "d"(a2), "S"(a3), "D"(a4)
                     : "memory");
    (void)a5;
    return ret;
}
#else
static inline long syscall6(long n, long a0, long a1, long a2,
                            long a3, long a4, long a5) {
    register long ret asm("a0");
    register long r_a0 asm("a0") = a0;
    register long r_a1 asm("a1") = a1;
    register long r_a2 asm("a2") = a2;
    register long r_a3 asm("a3") = a3;
    register long r_a7 asm("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(r_a0)
                     : "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a7)
                     : "memory");
    ret = r_a0;
    (void)a3;
    (void)a4;
    (void)a5;
    return ret;
}
#endif

static inline long sys_agent_create(void (*entry)(void), const char *name, uint32_t caps) {
    return syscall6(SYS_AGENT_CREATE, (long)entry, (long)name, (long)caps, 0, 0, 0);
}

static inline long sys_agent_destroy(int id) {
    return syscall6(SYS_AGENT_DESTROY, id, 0, 0, 0, 0, 0);
}

static inline long sys_agent_send(int dst, int type, const char *payload) {
    return syscall6(SYS_AGENT_SEND, dst, type, (long)payload, 0, 0, 0);
}

static inline long sys_agent_recv(struct agent_msg *msg) {
    return syscall6(SYS_AGENT_RECV, (long)msg, 0, 0, 0, 0, 0);
}

static inline long sys_agent_yield(void) {
    return syscall6(SYS_AGENT_YIELD, 0, 0, 0, 0, 0, 0);
}

static inline long sys_agent_self(void) {
    return syscall6(SYS_AGENT_SELF, 0, 0, 0, 0, 0, 0);
}

static inline long sys_agent_write(const char *buf, long len) {
    return syscall6(SYS_AGENT_WRITE, (long)buf, len, 0, 0, 0, 0);
}

static inline long sys_agent_exit(int code) {
    return syscall6(SYS_AGENT_EXIT, code, 0, 0, 0, 0, 0);
}

static inline long sys_agent_tool(int tool, long arg0, long arg1, long arg2) {
    return syscall6(SYS_AGENT_TOOL, tool, arg0, arg1, arg2, 0, 0);
}

static inline long sys_agent_llm(const char *prompt, char *buf, long buflen) {
    return syscall6(SYS_AGENT_LLM, (long)prompt, (long)buf, buflen, 0, 0, 0);
}

static inline long sys_agent_compact(int keep) {
    return syscall6(SYS_AGENT_COMPACT, keep, 0, 0, 0, 0, 0);
}

static inline long sys_agent_get_phase(void) {
    return syscall6(SYS_AGENT_GET_PHASE, 0, 0, 0, 0, 0, 0);
}

static inline long sys_agent_sync(void) {
    return syscall6(SYS_AGENT_SYNC, 0, 0, 0, 0, 0, 0);
}

static inline long sys_agent_heap_base(void) {
    return syscall6(SYS_AGENT_HEAP_BASE, 0, 0, 0, 0, 0, 0);
}

static inline long sys_agent_heap_map(int page_off) {
    return syscall6(SYS_AGENT_HEAP_MAP, page_off, 0, 0, 0, 0, 0);
}

static inline long sys_agent_heap_cow(int dst_off, int src_off) {
    return syscall6(SYS_AGENT_HEAP_COW, dst_off, src_off, 0, 0, 0, 0);
}

static inline long sys_agent_load(const char *path, const char *name, uint32_t caps) {
    return syscall6(SYS_AGENT_LOAD, (long)path, (long)name, (long)caps, 0, 0, 0);
}

static inline void *agent_heap_base(void) {
    return (void *)sys_agent_heap_base();
}

static inline int agent_heap_map_page(int page_off) {
    return (int)sys_agent_heap_map(page_off);
}

static inline int agent_heap_cow_page(int dst_off, int src_off) {
    return (int)sys_agent_heap_cow(dst_off, src_off);
}

#endif
