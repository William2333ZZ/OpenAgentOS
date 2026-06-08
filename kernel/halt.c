#include "halt.h"
#include "printf.h"

__attribute__((noreturn)) void kernel_halt(void) {
    kprintf("[kernel] shutdown\n");
    __asm__ volatile(
        "li a0, 0\n"
        "li a1, 0\n"
        "li a7, 8\n"
        "ecall\n"
        :
        :
        : "a0", "a1", "a7", "memory");
    __asm__ volatile(
        "li a0, 0\n"
        "li a1, 0\n"
        "li a6, 0\n"
        "li a7, 0x53525354\n"
        "ecall\n"
        :
        :
        : "a0", "a1", "a6", "a7", "memory");
    for (;;)
        __asm__ volatile("wfi");
}
