#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>

#define GPU_FB_WIDTH  320
#define GPU_FB_HEIGHT 240

void virtio_gpu_init(void);
int virtio_gpu_ready(void);
void virtio_gpu_clear(uint32_t color);
void virtio_gpu_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg);
int virtio_gpu_flush(void);

#endif
