#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "persist.h"
#include "ramfs.h"
#include "sched.h"
#include "smp.h"
#include "timer.h"
#include "tool.h"
#include "vm.h"
#include "uart.h"

#ifdef PLATFORM_X86_64_PC
#include "boot_x86.h"
#else
#include "boot_riscv.h"
#endif

#ifndef INIT_AGENT
#define INIT_AGENT init_wrap_agent
#endif

#ifdef PLATFORM_X86_64_PC
void kmain(void) {
    smp_boot(0, 0);
#else
void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);
#endif

    agentos_boot_banner("v2.x wrap-up");

    agentos_trap_init();

    mem_init();
    vm_init();
    smp_ipi_init();
    persist_init();
    ramfs_init();
    if (persist_load() != 0)
        kprintf("[kernel] persist_load failed, using fresh ramfs\n");
    tool_init();
    agent_init();

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_FS |
                 CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    scheduler_run_first(init);
}
