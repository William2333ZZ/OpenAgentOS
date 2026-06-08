#ifndef AGENT_ARCH_H
#define AGENT_ARCH_H

#include "agent.h"

void agent_arch_init_tf(struct trapframe *tf, void (*entry)(void), unsigned long user_sp);

#endif
