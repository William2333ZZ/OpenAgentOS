#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "smp.h"
#include "timer.h"
#include "tool.h"
#include "vm.h"
#include "boot_riscv.h"
#include "halt.h"

#ifndef INIT_AGENT
#define INIT_AGENT init_smp_agent
#endif

void kmain(unsigned long hartid, unsigned long dtb) {
    int i;

    smp_boot(hartid, dtb);

    agentos_boot_banner("SMP bring-up");
    agentos_trap_init();

    mem_init();
    vm_init();
    smp_ipi_init();
    ramfs_init();
    tool_init();
    agent_init();

    smp_wake_harts(2);
    smp_bsp_release();

    for (i = 0; i < 50000000 && smp_online_count() < 2; i++)
        __asm__ volatile("nop");

    kprintf("[smp] boot hart=%d online harts=%d\n", (int)hartid, smp_online_count());
    if (smp_online_count() < 2) {
        kprintf("[smp] expected >=2 harts (run QEMU with -smp 2)\n");
        kernel_halt();
    }

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV;

    scheduler_run_first(init);
}
