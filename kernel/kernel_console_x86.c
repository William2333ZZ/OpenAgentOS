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
#include "smp.h"
#include "boot_x86.h"
#include "virtio_gpu.h"
#include "virtio_input.h"
#include "vm.h"

#ifndef INIT_AGENT
#define INIT_AGENT init_console_quota_agent
#endif

#ifndef DEMO_TITLE
#define DEMO_TITLE "v6.4 x86 Console quota demo"
#endif

#ifndef CONSOLE_TAG
#define CONSOLE_TAG "x86 resource quota"
#endif

extern void display_agent_main(void);
extern void input_agent_main(void);
extern void console_agent_main(void);

void kmain(void) {
    smp_boot(0, 0);

    agentos_boot_banner(DEMO_TITLE);
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
    tenant_init();
    audit_init();
    namespace_init();
    quota_init();
    tool_init();
    agent_init();

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

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_FS | CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] console id=%d (%s)\n", CONSOLE_AGENT_ID, CONSOLE_TAG);
    scheduler_run_first(init);
}
