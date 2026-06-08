#include "halt.h"

__attribute__((noreturn)) void kernel_halt(void) {
    const char *s = "[kernel] shutdown\n";
    while (*s) {
        __asm__ volatile("outb %0, %1" : : "a"(*s), "Nd"((unsigned short)0x3f8));
        s++;
    }
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0), "Nd"((unsigned short)0xf4));
    for (;;)
        __asm__ volatile("hlt");
}
