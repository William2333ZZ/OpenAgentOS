#include "../../agent_arch.h"

#define SSTATUS_SPP (1UL << 8)
#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SUM  (1UL << 18)

void agent_arch_init_tf(struct trapframe *tf, void (*entry)(void), unsigned long user_sp) {
    unsigned long gp;

    __asm__ volatile("mv %0, gp" : "=r"(gp));
    tf->sepc = (unsigned long)entry;
    tf->sp = user_sp;
    tf->gp = gp;
    tf->sstatus = SSTATUS_SPIE | SSTATUS_SUM;
    tf->sstatus &= ~SSTATUS_SPP;
}
