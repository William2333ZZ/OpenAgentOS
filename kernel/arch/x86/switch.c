#include "agent.h"
#include "printf.h"

__attribute__((naked)) static void switch_to_agent_asm(struct trapframe *tf) {
    __asm__ volatile(
        "mov 4(%esp), %edi\n"
        "mov 124(%edi), %eax\n"
        "mov 4(%edi), %ebx\n"
        "mov 128(%edi), %ecx\n"
        "pushl $0x23\n"
        "pushl %ebx\n"
        "pushl %ecx\n"
        "pushl $0x1b\n"
        "pushl %eax\n"
        "mov $0x23, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "iret\n");
}

void switch_to_agent(struct trapframe *tf) {
    switch_to_agent_asm(tf);
}

void agent_save_context(struct trapframe *tf) {
    (void)tf;
}
