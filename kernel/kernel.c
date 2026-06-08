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
#ifdef ENABLE_VIRTIO
#include "virtio.h"
#endif

#ifndef INIT_AGENT
#define INIT_AGENT init_agent
#endif

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

#if defined(ENABLE_VIRTIO)
    agentos_boot_banner("v0.5 VirtIO LLM Stream + Harness");
#elif defined(LLM_FAUX)
    agentos_boot_banner("v0.5 LLM Faux + Harness IPC");
#else
    agentos_boot_banner("v0.6 System Tools + Harness IPC");
#endif
    agentos_trap_init();

    mem_init();
    vm_init();
    ramfs_init();
    tool_init();
    agent_init();

#ifdef ENABLE_VIRTIO
    kprintf("[kernel] llm channel deferred until agent_llm()\n");
#endif

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);

    kprintf("[kernel] boot complete, starting init agent\n");
    scheduler_run_first(init);
}
