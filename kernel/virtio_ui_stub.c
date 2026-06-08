#include "virtio_gpu.h"
#include "virtio_input.h"

__attribute__((weak)) void virtio_gpu_init(void) {}
__attribute__((weak)) int virtio_gpu_ready(void) { return 0; }
__attribute__((weak)) void virtio_gpu_clear(uint32_t color) { (void)color; }
__attribute__((weak)) void virtio_gpu_draw_text(int x, int y, const char *text, uint32_t fg,
                                                uint32_t bg) {
    (void)x;
    (void)y;
    (void)text;
    (void)fg;
    (void)bg;
}
__attribute__((weak)) int virtio_gpu_flush(void) { return -6; }

__attribute__((weak)) void virtio_input_init(void) {}
__attribute__((weak)) int virtio_input_ready(void) { return 0; }
__attribute__((weak)) int virtio_input_poll(int *type, int *code, int *value) {
    (void)type;
    (void)code;
    (void)value;
    return -6;
}
