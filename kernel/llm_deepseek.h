#ifndef LLM_DEEPSEEK_H
#define LLM_DEEPSEEK_H

typedef void (*llm_delta_fn)(const char *chunk, int len, void *ctx);

int llm_deepseek_query(const char *prompt, char *buf, int buflen, llm_delta_fn on_delta,
                       void *ctx);

#endif
