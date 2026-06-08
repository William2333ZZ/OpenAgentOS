#include "../include/platform.h"
#include "uart.h"
#include "halt.h"

void kmain(void) {
    platform_init();
    uart_init();
    boot_banner("x86 platform smoke");
    uart_puts("[x86] platform smoke ok\n");
    kernel_halt();
}
