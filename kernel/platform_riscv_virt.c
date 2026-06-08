#include "../include/platform.h"

const struct agentos_platform platform_riscv_virt = {
    .name = "riscv64-virt",
    .arch = "riscv64",
    .uart_mmio = 0x10000000UL,
    .virtio_mmio_base = 0x10001000UL,
    .virtio_mmio_stride = 0x1000UL,
    .virtio_mmio_count = 8,
    .ram_base = 0x80000000UL,
    .heap_start = 0x80600000UL,
    .kernel_load = 0x80200000UL,
};
