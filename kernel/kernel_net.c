#include "agent.h"
#include "http.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "timer.h"
#include "tool.h"
#include "smp.h"
#include "boot_riscv.h"
#include "virtio_net.h"
#include "vm.h"

#ifndef INIT_AGENT
#define INIT_AGENT init_net_agent
#endif

extern void network_agent_main(void);

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v3.4 Network service demo");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    virtio_net_init();
    tool_init();
    agent_init();

    struct agent *net = &agents[NET_AGENT_ID];
    net->id = NET_AGENT_ID;
    agent_start_service(net, network_agent_main, "network",
                        CAP_LOG | CAP_SVC_NET | CAP_NET | CAP_SEND | CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_TIME |
                 CAP_FS;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] http requests via network id=%d\n", NET_AGENT_ID);
    scheduler_run_first(init);
}
