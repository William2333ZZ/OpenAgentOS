#ifndef VIRTIO_H
#define VIRTIO_H

#include <stdint.h>

#define LLM_MAX_PROMPT   256
#define LLM_MAX_RESPONSE 512

typedef void (*llm_delta_fn)(const char *chunk, int len, void *ctx);

int virtio_console_init(void);
int virtio_console_ready(void);
int virtio_llm_exchange(const char *prompt, char *response, int response_cap);
int virtio_llm_stream(const char *prompt, char *response, int response_cap,
                      llm_delta_fn on_delta, void *ctx);
int virtio_http_exchange(const char *tx, unsigned long tx_len, char *rx,
                         unsigned long rx_cap);

#endif
