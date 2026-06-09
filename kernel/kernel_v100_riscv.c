#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "timer.h"
#include "tool.h"
#include "tenant.h"
#include "audit.h"
#include "namespace.h"
#include "quota.h"
#include "fleet.h"
#include "policy.h"
#include "remote.h"
#include "mesh.h"
#include "netstack.h"
#include "smp.h"
#include "boot_riscv.h"
#include "virtio_gpu.h"
#include "virtio_input.h"
#include "vm.h"

extern void router_agent_main(void);
extern void display_agent_main(void);
extern void input_agent_main(void);
extern void console_agent_main(void);
extern void init_v100_agent(void);

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("1.0.0 — virtio-net + GA LTS sandbox runtime");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    virtio_gpu_init();
    virtio_input_init();
    if (!virtio_gpu_ready())
        kprintf("[kernel] ui: no GPU — display falls back to console (UART)\n");
    if (!virtio_input_ready())
        kprintf("[kernel] ui: no virtio-keyboard — input falls back to UART\n");
    netstack_init();
    if (!netstack_ready())
        kprintf("[kernel] net: virtio-net not ready\n");
    tenant_init();
    audit_init();
    namespace_init();
    quota_init();
    fleet_init();
    policy_init();
    remote_init();
    mesh_init();
    tool_init();
    agent_init();

    struct agent *router = &agents[ROUTER_AGENT_ID];
    router->id = ROUTER_AGENT_ID;
    agent_start_service(router, router_agent_main, "model-router",
                        CAP_LOG | CAP_SVC_LLM | CAP_LLM | CAP_SEND | CAP_RECV);

    struct agent *display = &agents[DISPLAY_AGENT_ID];
    display->id = DISPLAY_AGENT_ID;
    agent_start_service(display, display_agent_main, "display",
                        CAP_LOG | CAP_SVC_DISPLAY | CAP_DISPLAY | CAP_SEND | CAP_RECV);

    struct agent *input = &agents[INPUT_AGENT_ID];
    input->id = INPUT_AGENT_ID;
    agent_start_service(input, input_agent_main, "input",
                        CAP_LOG | CAP_SVC_INPUT | CAP_INPUT | CAP_SEND | CAP_RECV);

    struct agent *console = &agents[CONSOLE_AGENT_ID];
    console->id = CONSOLE_AGENT_ID;
    agent_start_service(console, console_agent_main, "console",
                        CAP_LOG | CAP_SVC_CONSOLE | CAP_SEND | CAP_RECV);

    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, init_v100_agent);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_FS |
                 CAP_TIME | CAP_GPIO;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] router id=%d console id=%d (1.0.0 DeepSeek)\n",
            ROUTER_AGENT_ID, CONSOLE_AGENT_ID);
    scheduler_run_first(init);
}
