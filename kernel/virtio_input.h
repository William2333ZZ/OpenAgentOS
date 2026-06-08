#ifndef VIRTIO_INPUT_H
#define VIRTIO_INPUT_H

#include <stdint.h>

#define INPUT_EV_SYN 0
#define INPUT_EV_KEY 1

void virtio_input_init(void);
int virtio_input_ready(void);
/* Returns 1 if event read, 0 if none, negative on error. */
int virtio_input_poll(int *type, int *code, int *value);

#endif
