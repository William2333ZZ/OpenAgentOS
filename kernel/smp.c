#include "smp.h"
#include "trap.h"
#include "printf.h"
#include "halt.h"
#include "sched.h"

static volatile int online_harts;
static int boot_hart;
static volatile int bsp_ready;

static volatile unsigned long hart_ticks[SMP_MAX_HARTS];
static volatile int hart_ready[SMP_MAX_HARTS];

#define SBI_EXT_HSM 0x48534D
#define SBI_HSM_HART_START 0
#define SBI_EXT_IPI 0x735049
#define SBI_IPI_SEND_IPI 0

struct sbiret {
    long error;
    long value;
};

static struct sbiret sbi_ecall(long ext, long fid, long arg0, long arg1, long arg2) {
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a2), "r"(a6), "r"(a7)
                     : "memory");
    return (struct sbiret){ .error = a0, .value = a1 };
}

extern char _start[];

void smp_ipi_clear(int hart) {
    unsigned long sip;

    if (hart != smp_hart_id())
        return;
    __asm__ volatile("csrr %0, sip" : "=r"(sip));
    (void)sip;
    __asm__ volatile("csrc sip, %0" : : "r"(1UL << 1));
}

void smp_kick_cpu(int hart) {
    unsigned long mask;

    if (hart < 0 || hart >= SMP_MAX_HARTS)
        return;
    if (hart == smp_hart_id())
        return;
    mask = 1UL << (unsigned long)hart;
    sbi_ecall(SBI_EXT_IPI, SBI_IPI_SEND_IPI, (long)mask, 0, 0);
}

void smp_kick_others(int except_hart) {
    int h;

    for (h = 0; h < SMP_MAX_HARTS; h++) {
        if (h == except_hart)
            continue;
        if (h < online_harts)
            smp_kick_cpu(h);
    }
}

void smp_cpu_wait(void) {
    int h = smp_hart_id();
    int i;

    smp_ipi_clear(h);
    for (i = 0; i < 500000; i++) {
        if (sched_has_runnable())
            return;
        __asm__ volatile("" ::: "memory");
    }
    __asm__ volatile("wfi");
    smp_ipi_clear(h);
}

void smp_init(void) {
    int i;

    online_harts = 1;
    boot_hart = 0;
    bsp_ready = 0;
    for (i = 0; i < SMP_MAX_HARTS; i++) {
        hart_ticks[i] = 0;
        hart_ready[i] = 0;
    }
    hart_ready[0] = 1;
}

void smp_ipi_init(void) {
    unsigned long sie;

    __asm__ volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1UL << 1);
    __asm__ volatile("csrw sie, %0" : : "r"(sie));
}

int smp_boot(unsigned long hartid, unsigned long dtb) {
    (void)dtb;
    if (hartid != 0) {
        smp_secondary_entry(hartid);
        __builtin_unreachable();
    }
    smp_init();
    return 0;
}

void smp_bsp_release(void) {
    __sync_synchronize();
    bsp_ready = 1;
    smp_kick_others(-1);
}

void smp_secondary_entry(unsigned long hartid) {
    extern void sched_cpu_run_loop(void);

    if ((int)hartid >= SMP_MAX_HARTS) {
        for (;;)
            __asm__ volatile("wfi");
    }

    while (!bsp_ready)
        __asm__ volatile("wfi");

    trap_init();
    __sync_fetch_and_add(&online_harts, 1);
    hart_ticks[hartid] = 1;
    hart_ready[hartid] = 1;

    sched_cpu_run_loop();
    for (;;)
        __asm__ volatile("wfi");
}

int smp_online_count(void) {
    return online_harts;
}

int smp_boot_hart(void) {
    return boot_hart;
}

unsigned long smp_hart_ticks(int hartid) {
    if (hartid < 0 || hartid >= SMP_MAX_HARTS)
        return 0;
    return hart_ticks[hartid];
}

int smp_hart_ready(int hartid) {
    if (hartid < 0 || hartid >= SMP_MAX_HARTS)
        return 0;
    return hart_ready[hartid];
}

int smp_hart_id(void) {
    unsigned long id;

    __asm__ volatile("csrr %0, sscratch" : "=r"(id));
    return (int)id;
}

void smp_wake_harts(int target) {
    int h;

    if (target > SMP_MAX_HARTS)
        target = SMP_MAX_HARTS;
    for (h = 1; h < target; h++) {
        struct sbiret r =
            sbi_ecall(SBI_EXT_HSM, SBI_HSM_HART_START, h, (long)_start, 0);
        if (r.error != 0)
            kprintf("[smp] hart_start %d failed err=%ld\n", h, r.error);
    }
}
