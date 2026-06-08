#include "../../agent_arch.h"

#define X86_EFLAGS_IF (1UL << 9)

void agent_arch_init_tf(struct trapframe *tf, void (*entry)(void), unsigned long user_sp) {
    tf->sepc = (unsigned long)entry;
    tf->sp = user_sp;
    tf->gp = 0;
    tf->t6 = 0x1bUL;
    tf->sstatus = 0x202UL;
}
