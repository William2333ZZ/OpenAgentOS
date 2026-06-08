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
#define INIT_AGENT init_router_agent
#endif

extern void router_agent_main(void);

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v3.2 Model router demo");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    tool_init();
    agent_init();

    struct agent *router = &agents[ROUTER_AGENT_ID];
    router->id = ROUTER_AGENT_ID;
    agent_start_service(router, router_agent_main, "model-router",
                        CAP_LOG | CAP_SVC_LLM | CAP_LLM | CAP_SEND | CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] llm requests via router id=%d\n", ROUTER_AGENT_ID);
    scheduler_run_first(init);
}
