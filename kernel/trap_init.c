#include "trap.h"

void trap_init(void) {
    extern void trap_vector(void);
    unsigned long stvec = (unsigned long)trap_vector;

    __asm__ volatile("csrw stvec, %0" : : "r"(stvec));
}
