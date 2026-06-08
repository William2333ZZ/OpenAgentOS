#include "smp.h"
#include "sched.h"

static int online_harts = 1;
static int boot_hart;

void smp_init(void) {
    online_harts = 1;
    boot_hart = 0;
}

void smp_ipi_init(void) {}

int smp_boot(unsigned long hartid, unsigned long dtb) {
    (void)hartid;
    (void)dtb;
    smp_init();
    return 0;
}

void smp_secondary_entry(unsigned long hartid) {
    (void)hartid;
}

int smp_online_count(void) {
    return online_harts;
}

int smp_boot_hart(void) {
    return boot_hart;
}

void smp_wake_harts(int target) {
    (void)target;
}

unsigned long smp_hart_ticks(int hartid) {
    (void)hartid;
    return 0;
}

int smp_hart_ready(int hartid) {
    return hartid == 0;
}

int smp_hart_id(void) {
    return 0;
}

void smp_kick_cpu(int hart) {
    (void)hart;
}

void smp_kick_others(int except_hart) {
    (void)except_hart;
}

void smp_ipi_clear(int hart) {
    (void)hart;
}

void smp_cpu_wait(void) {
    int i;
    for (i = 0; i < 500000; i++) {
        if (sched_has_runnable())
            return;
        __asm__ volatile("" ::: "memory");
    }
    __asm__ volatile("hlt");
}

void smp_bsp_release(void) {}
