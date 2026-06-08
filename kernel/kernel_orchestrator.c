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
#define INIT_AGENT init_orchestrator_agent
#endif

extern void storage_agent_main(void);

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v1.3 Orchestrator demo");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    tool_init();
    agent_init();

    struct agent *storage = &agents[STORAGE_AGENT_ID];
    storage->id = STORAGE_AGENT_ID;
    agent_start_service(storage, storage_agent_main, "storage",
                        CAP_LOG | CAP_SVC_STORAGE | CAP_FS | CAP_SEND |
                            CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] llm channel deferred until agent_llm()\n");
    scheduler_run_first(init);
}
