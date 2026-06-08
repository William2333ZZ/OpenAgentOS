#include "sched.h"
#include "ipc.h"
#include "halt.h"
#include "printf.h"
#include "timer.h"
#include "vm.h"
#include "agent.h"
#include "spinlock.h"
#include "smp.h"

extern void switch_to_agent(struct trapframe *tf);

static int rr_next;
static volatile int kernel_sched_ready;
static spinlock_t sched_lock;
static int sched_lock_inited;
static struct agent *cpu_current[SMP_MAX_HARTS];
static int cpu_rq[SMP_MAX_HARTS][MAX_AGENTS];
static int cpu_rq_count[SMP_MAX_HARTS];

struct agent *agent_current(void) {
    int h = smp_hart_id();

    if (h < 0 || h >= SMP_MAX_HARTS)
        return 0;
    return cpu_current[h];
}

void agent_set_current(struct agent *a) {
    int h = smp_hart_id();

    if (h >= 0 && h < SMP_MAX_HARTS)
        cpu_current[h] = a;
}

static void sched_lock_init_once(void) {
    if (!sched_lock_inited) {
        spinlock_init(&sched_lock);
        sched_lock_inited = 1;
    }
}

static void trapframe_store(struct agent *a, struct trapframe *src) {
    if (a && a->tf && src)
        *a->tf = *src;
}

static struct trapframe *trapframe_sync(struct trapframe *src) {
    trapframe_store(agent_current(), src);
    if (!agent_current() || !agent_current()->tf) {
        kprintf("[sched] trapframe_sync without current agent\n");
        kernel_halt();
        __builtin_unreachable();
    }
    return agent_current()->tf;
}

static void sched_halt_if_done(void) {
    if (smp_hart_id() != smp_boot_hart()) {
        for (;;)
#ifdef PLATFORM_X86_64_PC
            __asm__ volatile("hlt");
#else
            __asm__ volatile("wfi");
#endif
    }
    kprintf("[kernel] no runnable agents, halting\n");
    kernel_halt();
}

static int sched_agent_selectable(struct agent *a) {
    if (!a || a->state == AGENT_UNUSED || a->state == AGENT_ZOMBIE ||
        a->state == AGENT_CREATED)
        return 0;
    if (a->state == AGENT_RUNNING)
        return 0;
    return 1;
}

static void sched_rq_add_locked(struct agent *a) {
    int cpu = a->home_cpu;
    int i;

    if (cpu < 0 || cpu >= SMP_MAX_HARTS)
        cpu = 0;
    for (i = 0; i < cpu_rq_count[cpu]; i++) {
        if (cpu_rq[cpu][i] == a->id)
            return;
    }
    if (cpu_rq_count[cpu] >= MAX_AGENTS)
        return;
    cpu_rq[cpu][cpu_rq_count[cpu]++] = a->id;
}

static void sched_rq_remove_locked(int cpu, int id) {
    int i;
    int j;

    if (cpu < 0 || cpu >= SMP_MAX_HARTS)
        cpu = 0;
    for (i = 0; i < cpu_rq_count[cpu]; i++) {
        if (cpu_rq[cpu][i] != id)
            continue;
        for (j = i + 1; j < cpu_rq_count[cpu]; j++)
            cpu_rq[cpu][j - 1] = cpu_rq[cpu][j];
        cpu_rq_count[cpu]--;
        return;
    }
}

void sched_notify_runnable(struct agent *a) {
    if (!a || a->state == AGENT_ZOMBIE)
        return;
    sched_lock_init_once();
    spinlock_acquire(&sched_lock);
    a->state = AGENT_RUNNABLE;
    a->running_cpu = -1;
    sched_rq_add_locked(a);
    spinlock_release(&sched_lock);
    smp_kick_cpu(a->home_cpu);
    if (smp_online_count() > 1)
        smp_kick_others(smp_hart_id());
}

int sched_has_runnable(void) {
    int i;

    for (i = 0; i < MAX_AGENTS; i++) {
        struct agent *a = &agents[i];
        if (!sched_agent_selectable(a))
            continue;
        if (a->state == AGENT_RUNNABLE)
            return 1;
        if (!msgbox_empty(&a->inbox))
            return 1;
    }
    return 0;
}

static int sched_system_done(void) {
    int i;
    int active = 0;

    for (i = 1; i < MAX_AGENTS; i++) {
        struct agent *a = &agents[i];
        if (a->state == AGENT_UNUSED)
            continue;
        if (a->state != AGENT_ZOMBIE)
            active = 1;
    }
    return !active && !sched_has_runnable();
}

static struct agent *pick_from_runqueues(int hart) {
    int i;

    if (hart < 0 || hart >= SMP_MAX_HARTS)
        return 0;
    for (i = 0; i < cpu_rq_count[hart]; i++) {
        int id = cpu_rq[hart][i];
        struct agent *a = agent_get(id);
        if (a && a->state == AGENT_RUNNABLE && a->running_cpu < 0)
            return a;
    }
    return 0;
}

static int sched_agent_has_inbox(struct agent *a) {
    if (!a || a->state == AGENT_UNUSED || a->state == AGENT_ZOMBIE ||
        a->state == AGENT_CREATED)
        return 0;
    return 1;
}

static struct agent *pick_next(void) {
    int i;
    int hart = smp_hart_id();
    struct agent *a;

    for (i = 0; i < MAX_AGENTS; i++) {
        struct agent *x = &agents[i];
        if (!sched_agent_has_inbox(x))
            continue;
        if (x->home_cpu != hart)
            continue;
        if (!msgbox_empty(&x->inbox)) {
            x->state = AGENT_RUNNABLE;
            x->running_cpu = -1;
            return x;
        }
    }

    a = pick_from_runqueues(hart);
    if (a)
        return a;

    for (int pass = 0; pass < MAX_AGENTS; pass++) {
        int idx = (rr_next + pass) % MAX_AGENTS;
        struct agent *x = &agents[idx];
        if (!sched_agent_selectable(x))
            continue;
        if (x->home_cpu != hart)
            continue;
        if (x->state == AGENT_RUNNABLE) {
            rr_next = (idx + 1) % MAX_AGENTS;
            return x;
        }
    }
    return 0;
}

struct trapframe *schedule_trap(struct trapframe *cur_tf) {
    struct agent *next;
    struct agent *cur = agent_current();

    sched_lock_init_once();

    for (;;) {
        spinlock_acquire(&sched_lock);

        if (cur && cur_tf)
            trapframe_store(cur, cur_tf);

        if (cur && cur->state == AGENT_RUNNING) {
            cur->state = AGENT_RUNNABLE;
            cur->running_cpu = -1;
            sched_rq_add_locked(cur);
        }

        next = pick_next();
        if (next)
            break;

        if (sched_system_done()) {
            spinlock_release(&sched_lock);
            if (kernel_sched_ready) {
                spinlock_release(&sched_lock);
                sched_halt_if_done();
            }
            smp_cpu_wait();
            cur = agent_current();
            continue;
        }

        spinlock_release(&sched_lock);
        smp_cpu_wait();
        cur = agent_current();
    }

    agent_set_current(next);
    next->state = AGENT_RUNNING;
    next->running_cpu = smp_hart_id();
    sched_rq_remove_locked(next->home_cpu, next->id);
    if (next->pagetable)
        vm_activate(next->pagetable);
    if (!next->tf) {
        spinlock_release(&sched_lock);
        kprintf("[sched] agent %d has null trapframe\n", next->id);
        kernel_halt();
    }
    spinlock_release(&sched_lock);
    return next->tf;
}

void schedule(void) {
    struct trapframe *tf = schedule_trap(agent_current() ? agent_current()->tf : 0);
    if (agent_current() && agent_current()->pagetable)
        vm_activate(agent_current()->pagetable);
    switch_to_agent(tf);
}

void sched_cpu_run_loop(void) {
    struct agent *a;

    for (;;) {
        sched_lock_init_once();
        spinlock_acquire(&sched_lock);
        a = pick_next();
        if (!a) {
            if (kernel_sched_ready && sched_system_done()) {
                spinlock_release(&sched_lock);
                sched_halt_if_done();
            }
            spinlock_release(&sched_lock);
            smp_cpu_wait();
            continue;
        }
        agent_set_current(a);
        a->state = AGENT_RUNNING;
        a->running_cpu = smp_hart_id();
        sched_rq_remove_locked(a->home_cpu, a->id);
        if (a->pagetable)
            vm_activate(a->pagetable);
        if (!a->tf) {
            spinlock_release(&sched_lock);
            kprintf("[sched] agent %d has null trapframe\n", a->id);
            kernel_halt();
        }
        spinlock_release(&sched_lock);
        switch_to_agent(a->tf);
    }
}

void sched_start(struct agent *init) {
    int ncpu = smp_online_count();

    if (ncpu < 1)
        ncpu = 1;
    kernel_sched_ready = 1;
    __sync_synchronize();
    init->home_cpu = 0;
    init->running_cpu = smp_hart_id();
    init->state = AGENT_RUNNING;
    agent_set_current(init);
    kprintf("[kernel] boot complete, starting init agent (smp=%d)\n", ncpu);
    if (!init->tf) {
        kprintf("[kernel] invalid init agent state\n");
        kernel_halt();
    }
    if (init->pagetable)
        vm_activate(init->pagetable);
    switch_to_agent(init->tf);
    sched_cpu_run_loop();
}

void scheduler_run_first(struct agent *a) {
    if (!a || !a->tf) {
        kprintf("[kernel] invalid init agent state\n");
        kernel_halt();
    }
    if (smp_online_count() > 1) {
        sched_start(a);
        return;
    }
    agent_set_current(a);
    a->state = AGENT_RUNNING;
    a->running_cpu = smp_hart_id();
    kprintf("[kernel] start agent id=%d sepc=%x sp=%x\n", a->id,
            a->tf ? (unsigned int)a->tf->sepc : 0U,
            a->tf ? (unsigned int)a->tf->sp : 0U);
    if (a->pagetable)
        vm_activate(a->pagetable);
    switch_to_agent(a->tf);
}

