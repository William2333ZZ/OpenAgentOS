#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "timer.h"
#include "tool.h"
#include "smp.h"
#include "boot_riscv.h"
#include "vm.h"

#ifndef INIT_AGENT
#define INIT_AGENT init_bench_ipc_agent
#endif

extern void ping_agent_main(void);

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v5.5 IPC bench");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    tool_init();
    agent_init();

    struct agent *ping = &agents[2];
    ping->id = 2;
    agent_start_service(ping, ping_agent_main, "ping",
                        CAP_LOG | CAP_SEND | CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_TIME;

    kprintf("[kernel] boot complete, starting ipc bench (ping id=2)\n");
    scheduler_run_first(init);
}
