#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "timer.h"
#include "tool.h"
#include "smp.h"
#include "boot_riscv.h"
#include "virtio_gpu.h"
#include "virtio_input.h"
#include "vm.h"

#ifndef INIT_AGENT
#define INIT_AGENT init_desktop_agent
#endif

extern void display_agent_main(void);
extern void input_agent_main(void);
extern void shell_agent_main(void);

static void seed_sys_gpu(void) {
    if (virtio_gpu_ready())
        ramfs_put("/sys/gpu", "1", 2);
    else
        ramfs_put("/sys/gpu", "0", 2);
}

void kmain(unsigned long hartid, unsigned long dtb) {
    smp_boot(hartid, dtb);

    agentos_boot_banner("v5.4 Desktop Shell demo");
    agentos_trap_init();
    mem_init();
    vm_init();
    ramfs_init();
    virtio_gpu_init();
    virtio_input_init();
    seed_sys_gpu();
    if (!virtio_gpu_ready())
        kprintf("[kernel] ui: no GPU — shell will fall back to Console REPL\n");
    if (!virtio_input_ready())
        kprintf("[kernel] ui: no virtio-keyboard — input falls back to UART\n");
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

    struct agent *shell = &agents[SHELL_AGENT_ID];
    shell->id = SHELL_AGENT_ID;
    agent_start_service(shell, shell_agent_main, "shell",
                        CAP_LOG | CAP_SVC_SHELL | CAP_SEND | CAP_RECV);

    extern void INIT_AGENT(void);
    struct agent *init = &agents[1];
    init->id = 1;
    agent_start_kernel(init, INIT_AGENT);
    init->caps = CAP_LOG | CAP_MATH | CAP_SPAWN | CAP_SEND | CAP_RECV | CAP_FS | CAP_TIME;

    kprintf("[kernel] boot complete, starting init agent\n");
    kprintf("[kernel] shell id=%d (desktop launcher)\n", SHELL_AGENT_ID);
    scheduler_run_first(init);
}
