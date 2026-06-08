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
#define INIT_AGENT init_console_pack_agent
#endif

extern void console_agent_main(void);
extern void init_console_pack_agent(void);

static void seed_headless_sys(void) {
    ramfs_put("/sys/headless", "1", 2);
    ramfs_put("/sys/gpu", "0", 2);
}

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v5.5 Headless Box demo");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    seed_headless_sys();
    kprintf("[kernel] headless: no display/input services (UART console only)\n");
    tool_init();
    agent_init();

    struct agent *console = &agents[CONSOLE_AGENT_ID];
    console->id = CONSOLE_AGENT_ID;
    agent_start_service(console, console_agent_main, "console",
                        CAP_LOG | CAP_SVC_CONSOLE | CAP_SEND | CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_FS | CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] console id=%d (headless UART REPL)\n", CONSOLE_AGENT_ID);
    scheduler_run_first(init);
}
