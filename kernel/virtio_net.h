#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <stdint.h>

void virtio_net_init(void);
int virtio_net_ready(void);
const uint8_t *virtio_net_mac(void);
int virtio_net_send_frame(const void *frame, int len);
int virtio_net_poll_frame(void *frame, int frame_cap, int *out_len);
void virtio_net_poll(void);

#endif
