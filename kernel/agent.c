#include "agent.h"
#include "agent_arch.h"
#include "mem.h"
#include "persist.h"
#include "printf.h"
#include "sched.h"
#include "smp.h"
#include "uart.h"
#include "tool.h"
#include "quota.h"
#include "uaccess.h"
#define AGENT_WRITE_CHUNK 128

static void *memset_local(void *s, int c, unsigned long n) {
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

struct agent agents[MAX_AGENTS];
int agent_count;

static void strlcpy_local(char *dst, const char *src, int size) {
    int i;
    for (i = 0; i < size - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

void agent_init(void) {
    for (int i = 0; i < MAX_AGENTS; i++) {
        agents[i].id = i;
        agents[i].state = AGENT_UNUSED;
        agents[i].phase = AGENT_PHASE_IDLE;
        agents[i].name[0] = '\0';
        agents[i].stack_base = 0;
        agents[i].tf = 0;
        agents[i].caps = 0;
        agents[i].home_cpu = 0;
        agents[i].running_cpu = -1;
        msgbox_init(&agents[i].inbox);
    }
    agent_set_current(0);
    agent_count = 0;
}

void agent_set_phase(enum agent_phase phase) {
    if (current_agent)
        current_agent->phase = phase;
}

int agent_alloc_id(void) {
    for (int i = 1; i < MAX_AGENTS; i++) {
        if (agents[i].state == AGENT_UNUSED || agents[i].state == AGENT_ZOMBIE)
            return i;
    }
    return -1;
}

struct agent *agent_get(int id) {
    if (id < 0 || id >= MAX_AGENTS)
        return 0;
    if (agents[id].state == AGENT_UNUSED)
        return 0;
    return &agents[id];
}

static void agent_setup_stack(struct agent *a, void (*entry)(void)) {
    unsigned char *stack;
    unsigned long stack_pa;
    struct trapframe *tf;

    a->tf = 0;
    a->pagetable = 0;

    if (!kalloc_page())
        return;
    stack = kalloc(AGENT_STACK_SIZE);
    if (!stack)
        return;
    stack_pa = ((unsigned long)stack + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
#ifdef PLATFORM_X86_64_PC
    unsigned long stack_va = stack_pa;
    unsigned long user_sp = stack_pa + AGENT_STACK_SIZE - 256;
#else
    unsigned long stack_va = USER_STACK_BASE + (unsigned long)a->id * USER_STACK_SLOT +
                             PAGE_SIZE;
    unsigned long user_sp = stack_va + AGENT_STACK_SIZE - 256;
#endif

    a->pagetable = vm_create_agent_pt();
    if (!a->pagetable) {
        kprintf("[kernel] vm_create_agent_pt failed for agent %d\n", a->id);
        return;
    }
#ifdef PLATFORM_X86_64_PC
    if (vm_map_region(a->pagetable, stack_va, stack_pa, AGENT_STACK_SIZE,
                      PTE_U | PTE_R | PTE_W) != 0) {
        kprintf("[kernel] vm_map_user_stack failed for agent %d\n", a->id);
        a->pagetable = 0;
        return;
    }
#else
    if (vm_map_user_stack(a->pagetable, a->id, stack_pa, AGENT_STACK_SIZE) != 0) {
        kprintf("[kernel] vm_map_user_stack failed for agent %d\n", a->id);
        a->pagetable = 0;
        return;
    }
#endif

    tf = kalloc(sizeof(struct trapframe));
    if (!tf)
        return;
    memset_local(tf, 0, sizeof(*tf));
    tf->ra = 0;
    agent_arch_init_tf(tf, entry, user_sp);
    a->stack_base = stack_va + AGENT_STACK_SIZE;
    a->tf = tf;
    a->entry = entry;
    a->heap_base = USER_HEAP_BASE + (unsigned long)a->id * AGENT_HEAP_SIZE;
    a->heap_max_pages = (int)(AGENT_HEAP_SIZE / PAGE_SIZE);
    a->heap_mapped = 0;
}

int agent_has_cap(struct agent *a, unsigned int cap) {
    return a && (a->caps & cap);
}

int agent_create(void (*entry)(void), const char *name, unsigned int caps) {
    if (!current_agent || !agent_has_cap(current_agent, CAP_SPAWN))
        return EPERM;
    if (!entry || !name)
        return EINVAL;

    int id = agent_alloc_id();
    if (id < 0)
        return ENOSPC;

    struct agent *a = &agents[id];
    char kname[sizeof(a->name)];
    int nlen;

    a->id = id;
    agent_setup_stack(a, entry);
    if (!a->tf)
        return ENOSPC;
    nlen = user_strnlen(name, (int)sizeof(kname) - 1);
    if (nlen < 0)
        return EFAULT;
    if (nlen == 0)
        return EINVAL;
    if (copy_from_user(kname, name, (unsigned long)nlen + 1) < 0)
        return EFAULT;
    strlcpy_local(a->name, kname, sizeof(a->name));
    a->caps = caps;
    a->state = AGENT_RUNNABLE;
    a->phase = AGENT_PHASE_IDLE;
    a->exit_code = 0;
    {
        int ncpu = smp_online_count();
        if (ncpu < 1)
            ncpu = 1;
        a->home_cpu = id % ncpu;
    }
    a->running_cpu = -1;
    msgbox_init(&a->inbox);
    agent_count++;

    kprintf("[kernel] agent_create id=%d name=%s caps=%u cpu=%d\n", id, a->name, caps,
            a->home_cpu);
    sched_notify_runnable(a);
    return id;
}

int agent_send(int dst, int type, const char *payload) {
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_SEND))
        return EPERM;
    if (!payload)
        return EINVAL;

    struct agent *target = agent_get(dst);
    if (!target)
        return ENOENT;

    rc = quota_ipc_allow(current_agent);
    if (rc != 0)
        return rc;

    struct agent_msg msg;
    int len;

    msg.sender_id = current_agent->id;
    msg.type = type;
    memset_local(msg.payload, 0, sizeof(msg.payload));
    len = user_strnlen(payload, MSG_PAYLOAD_SIZE - 1);
    if (len < 0)
        return EFAULT;
    if (len == 0)
        return EINVAL;
    if (copy_from_user(msg.payload, payload, (unsigned long)len) < 0)
        return EFAULT;
    msg.payload[len] = '\0';

    rc = msgbox_push(&target->inbox, &msg);
    if (rc)
        return rc;

    quota_ipc_charge(current_agent);

    if (target->state == AGENT_WAITING)
        kprintf("[kernel] wake agent %d (%s)\n", target->id, target->name);
    sched_notify_runnable(target);
    return 0;
}

int agent_send_kernel(struct agent *from, int dst, int type, const char *payload) {
    int rc;

    rc = quota_ipc_allow(from);
    if (rc != 0)
        return rc;
    rc = agent_send_kernel_nocheck(from, dst, type, payload);
    if (rc == 0)
        quota_ipc_charge(from);
    return rc;
}

int agent_send_kernel_nocheck(struct agent *from, int dst, int type,
                              const char *payload) {
    struct agent *target;
    struct agent_msg msg;
    int rc;
    int len;

    if (!from || !payload)
        return EINVAL;
    if (!agent_has_cap(from, CAP_SEND))
        return EPERM;

    target = agent_get(dst);
    if (!target)
        return ENOENT;

    msg.sender_id = from->id;
    msg.type = type;
    memset_local(msg.payload, 0, sizeof(msg.payload));
    for (len = 0; payload[len] && len < MSG_PAYLOAD_SIZE - 1; len++)
        msg.payload[len] = payload[len];
    if (len <= 0)
        return EINVAL;
    msg.payload[len] = '\0';

    rc = msgbox_push(&target->inbox, &msg);
    if (rc)
        return rc;

    if (target->state == AGENT_WAITING)
        kprintf("[kernel] wake agent %d (%s)\n", target->id, target->name);
    sched_notify_runnable(target);
    return 0;
}

int agent_recv(struct agent_msg *msg) {
    if (!current_agent || !agent_has_cap(current_agent, CAP_RECV))
        return EPERM;
    if (!msg)
        return EINVAL;

    struct agent_msg kmsg;

    if (msgbox_empty(&current_agent->inbox)) {
        agent_set_phase(AGENT_PHASE_WAIT_IPC);
        return EAGAIN;
    }

    if (msgbox_pop(&current_agent->inbox, &kmsg) != 0)
        return EAGAIN;

    if (copy_to_user(msg, &kmsg, sizeof(kmsg)) < 0) {
        (void)msgbox_push(&current_agent->inbox, &kmsg);
        return EFAULT;
    }
    agent_set_phase(AGENT_PHASE_TURN);
    return kmsg.type;
}

int agent_compact_inbox(int keep) {
    if (!current_agent || !agent_has_cap(current_agent, CAP_RECV))
        return EPERM;
    return msgbox_compact(&current_agent->inbox, keep);
}

int agent_get_phase(void) {
    if (!current_agent)
        return EPERM;
    return (int)current_agent->phase;
}

int agent_destroy(int id) {
    struct agent *target = agent_get(id);
    if (!target)
        return ENOENT;
    target->state = AGENT_ZOMBIE;
    kprintf("[kernel] agent_destroy id=%d\n", id);
    return 0;
}

void agent_exit(int code) {
    struct agent *a = agent_current();

    if (!a)
        return;
    a->exit_code = code;
    a->state = AGENT_ZOMBIE;
    a->running_cpu = -1;
    kprintf("[kernel] agent_exit id=%d code=%d hart=%d\n", a->id, code, smp_hart_id());
    if (smp_online_count() > 1)
        smp_kick_others(smp_hart_id());
}

int agent_tool(int tool, long arg0, long arg1, long arg2) {
    int rc;
    if (!current_agent)
        return EPERM;
    agent_set_phase(AGENT_PHASE_TOOL);
    rc = tool_dispatch(current_agent, tool, arg0, arg1, arg2);
    agent_set_phase(AGENT_PHASE_IDLE);
    return rc;
}

int agent_write(const char *buf, long len) {
    char kbuf[AGENT_WRITE_CHUNK];
    long i;

    if (!current_agent || !agent_has_cap(current_agent, CAP_LOG))
        return EPERM;
    if (!buf || len <= 0)
        return EINVAL;

    for (i = 0; i < len; i += AGENT_WRITE_CHUNK) {
        long chunk = len - i;
        long j;

        if (chunk > AGENT_WRITE_CHUNK)
            chunk = AGENT_WRITE_CHUNK;
        if (copy_from_user(kbuf, buf + i, (unsigned long)chunk) < 0)
            return EFAULT;
        for (j = 0; j < chunk; j++)
            uart_putc(kbuf[j]);
    }
    return (int)len;
}

int agent_sync(void) {
    return persist_sync();
}

long agent_get_heap_base(void) {
    if (!current_agent)
        return EPERM;
    return (long)current_agent->heap_base;
}

int agent_heap_map(int page_off) {
    unsigned long va;

    if (!current_agent)
        return EPERM;
    if (page_off < 0 || page_off >= current_agent->heap_max_pages)
        return EINVAL;
    va = current_agent->heap_base + (unsigned long)page_off * PAGE_SIZE;
    if (vm_heap_map_page(current_agent, va) != 0)
        return ENOSPC;
    return (int)va;
}

int agent_heap_cow(int dst_off, int src_off) {
    if (!current_agent)
        return EPERM;
    return vm_heap_cow_alias(current_agent, dst_off, src_off);
}

int agent_start_kernel(struct agent *a, void (*entry)(void)) {
    agent_setup_stack(a, entry);
    if (!a->tf)
        return ENOSPC;
    a->state = AGENT_CREATED;
    a->phase = AGENT_PHASE_IDLE;
#ifdef ENABLE_PERSIST
    a->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_TIME;
#else
    a->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_LLM |
              CAP_FS | CAP_TIME;
#endif
    strlcpy_local(a->name, "init", sizeof(a->name));
    a->home_cpu = 0;
    a->running_cpu = -1;
    agent_count++;
    return 0;
}

int agent_start_service(struct agent *a, void (*entry)(void), const char *name,
                        unsigned int caps) {
    agent_setup_stack(a, entry);
    if (!a->tf)
        return ENOSPC;
    a->state = AGENT_RUNNABLE;
    a->phase = AGENT_PHASE_IDLE;
    a->caps = caps;
    a->exit_code = 0;
    if (name)
        strlcpy_local(a->name, name, sizeof(a->name));
    msgbox_init(&a->inbox);
    agent_count++;
    kprintf("[kernel] service agent id=%d name=%s caps=%u\n", a->id, a->name, caps);
    sched_notify_runnable(a);
    return 0;
}
