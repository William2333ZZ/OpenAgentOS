#include "sched.h"
#include "timer.h"
#include "vm.h"
#include "agent.h"
#include "halt.h"
#include "printf.h"
#include "smp.h"

#define X86_EFLAGS_IF (1UL << 9)

extern long sched_handle_syscall(long n, long a0, long a1, long a2, long a3);

static int timer_armed;

static void trapframe_store_local(struct agent *a, struct trapframe *src) {
    if (a && a->tf && src)
        *a->tf = *src;
}

static struct trapframe *trapframe_sync_local(struct trapframe *src) {
    trapframe_store_local(agent_current(), src);
    if (!agent_current() || !agent_current()->tf) {
        kprintf("[sched] trapframe_sync without current agent\n");
        kernel_halt();
        __builtin_unreachable();
    }
    return agent_current()->tf;
}

static void maybe_arm_timer(void) {
    if (!timer_armed) {
        timer_irq_unmask();
        timer_armed = 1;
    }
}

static struct trapframe *handle_syscall(struct trapframe *tf) {
    long n = (long)tf->a7;

    if (n == SYS_AGENT_YIELD) {
        tf->a7 = 0;
        trapframe_store_local(agent_current(), tf);
        struct trapframe *next = schedule_trap(tf);
        maybe_arm_timer();
        return next;
    }

    if (n == SYS_AGENT_EXIT) {
        agent_exit((int)tf->a0);
        trapframe_store_local(agent_current(), tf);
        struct trapframe *next = schedule_trap(tf);
        maybe_arm_timer();
        return next;
    }

    if (n == SYS_AGENT_SEND) {
        long ret = agent_send((int)tf->a0, (int)tf->a1, (const char *)tf->a2);
        tf->a7 = (unsigned long)ret;
        trapframe_store_local(agent_current(), tf);
        return trapframe_sync_local(tf);
    }

    long ret = sched_handle_syscall(n, (long)tf->a0, (long)tf->a1, (long)tf->a2, (long)tf->a3);
    tf->a7 = (unsigned long)ret;
    trapframe_store_local(agent_current(), tf);
    return trapframe_sync_local(tf);
}

struct trapframe *trap_handler(struct trapframe *tf) {
    struct trapframe local;
    unsigned long vec;
    unsigned long fault_addr;

    if (tf)
        local = *tf;
    tf = &local;

    vec = tf->ra;

    if (vec == 0x80)
        return handle_syscall(tf);

    if (vec == 0x20) {
        timer_ack();
        if (!(tf->sstatus & X86_EFLAGS_IF))
            return tf;
        if (agent_current()) {
            trapframe_store_local(agent_current(), tf);
            return schedule_trap(tf);
        }
        return tf;
    }

    if (vec == 0x0d) {
        kprintf("[kernel] GP fault eip=%x sp=%x\n",
                (unsigned int)tf->sepc, (unsigned int)tf->sp);
        kernel_halt();
    }

    if (vec == 0x0e) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        if (agent_current() && vm_heap_fault(agent_current(), fault_addr, 1)) {
            trapframe_store_local(agent_current(), tf);
            return agent_current()->tf;
        }
        if (agent_current()) {
            kprintf("[kernel] user fault kill agent=%d cr2=%lx eip=%lx\n",
                    agent_current()->id, fault_addr, tf->sepc);
            agent_exit(1);
            trapframe_store_local(agent_current(), tf);
            return schedule_trap(tf);
        }
        kprintf("[kernel] kernel page fault cr2=%lx eip=%lx\n", fault_addr, tf->sepc);
        kernel_halt();
        __builtin_unreachable();
    }

    kprintf("[kernel] unexpected trap vec=%lx eip=%lx\n", vec, tf->sepc);
    kernel_halt();
    __builtin_unreachable();
}
