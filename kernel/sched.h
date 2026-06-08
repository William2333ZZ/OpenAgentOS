#ifndef SCHED_H
#define SCHED_H

#include "agent.h"

void schedule(void);
struct trapframe *schedule_trap(struct trapframe *cur_tf);
void scheduler_run_first(struct agent *a);
void sched_start(struct agent *init);
void sched_cpu_run_loop(void);
void sched_notify_runnable(struct agent *a);
int sched_has_runnable(void);
struct trapframe *trap_handler(struct trapframe *tf);
void agent_save_context(struct trapframe *tf);

#endif
