#include "sched.h"
#include "timer.h"
#include "vm.h"
#include "agent.h"
#include "halt.h"
#include "printf.h"
#include "smp.h"

#define SCAUSE_ECALL_U 8
#define SCAUSE_SSI 0x8000000000000001UL
#define SCAUSE_TIMER_IRQ 0x8000000000000005UL
#define SCAUSE_INST_PAGE_FAULT 12
#define SCAUSE_LOAD_PAGE_FAULT 13
#define SCAUSE_STORE_PAGE_FAULT 15
#define SSTATUS_SPP (1UL << 8)

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
        timer_init();
        timer_armed = 1;
    }
}

static struct trapframe *handle_ecall(struct trapframe *tf) {
    long n = (long)tf->a7;
    unsigned long sstatus;

    __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus |= (1UL << 18);
    __asm__ volatile("csrw sstatus, %0" : : "r"(sstatus));

    tf->sepc += 4;

    if (n == SYS_AGENT_YIELD) {
        tf->a0 = 0;
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
        tf->a0 = (unsigned long)ret;
        trapframe_store_local(agent_current(), tf);
        return trapframe_sync_local(tf);
    }

    long ret = sched_handle_syscall(n, (long)tf->a0, (long)tf->a1, (long)tf->a2, (long)tf->a3);
    tf->a0 = (unsigned long)ret;
    trapframe_store_local(agent_current(), tf);
    return trapframe_sync_local(tf);
}

struct trapframe *trap_handler(struct trapframe *tf) {
    struct trapframe local;
    struct trapframe *stk = tf;
    unsigned long scause;
    unsigned long stval;
    unsigned long sepc;

    if (tf)
        local = *tf;
    tf = &local;

    __asm__ volatile("csrc sstatus, %0" : : "r"(2UL));

    __asm__ volatile("csrr %0, scause" : "=r"(scause));
    __asm__ volatile("csrr %0, stval" : "=r"(stval));
    __asm__ volatile("csrr %0, sepc" : "=r"(sepc));

    if (scause == SCAUSE_ECALL_U)
        return handle_ecall(tf);

    if (scause == SCAUSE_SSI) {
        smp_ipi_clear(smp_hart_id());
        return stk;
    }

    if (scause == SCAUSE_TIMER_IRQ) {
        timer_ack();
        if (tf->sstatus & SSTATUS_SPP) {
            if (agent_current())
                return agent_current()->tf;
            kernel_halt();
            __builtin_unreachable();
        }
        if (agent_current()) {
            trapframe_store_local(agent_current(), tf);
            return schedule_trap(tf);
        }
        return stk;
    }

    if (scause == SCAUSE_INST_PAGE_FAULT || scause == SCAUSE_LOAD_PAGE_FAULT ||
        scause == SCAUSE_STORE_PAGE_FAULT) {
        int write = (scause == SCAUSE_STORE_PAGE_FAULT);
        kprintf("[kernel] page fault agent=%d scause=%lx stval=%lx sepc=%lx\n",
                agent_current() ? agent_current()->id : -1, scause, stval, sepc);
        if (agent_current() && vm_heap_fault(agent_current(), stval, write)) {
            trapframe_store_local(agent_current(), tf);
            return agent_current()->tf;
        }
        if (agent_current()) {
            kprintf("[kernel] user fault kill agent=%d\n", agent_current()->id);
            agent_exit(1);
            trapframe_store_local(agent_current(), tf);
            return schedule_trap(tf);
        }
        kernel_halt();
        __builtin_unreachable();
    }

    kprintf("[kernel] unexpected trap scause=%lx stval=%lx sepc=%lx\n",
            scause, stval, sepc);
    kernel_halt();
    __builtin_unreachable();
}
