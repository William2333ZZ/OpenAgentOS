#include "llm.h"
#include "agent.h"
#include "session.h"
#include "virtio.h"
#include "printf.h"

#ifdef LLM_NATIVE_NET
#include "llm_deepseek.h"
#include "netstack.h"
#endif

static char llm_stream_buf[LLM_MAX_RESPONSE];
static int llm_stream_len;

static int contains_substr(const char *hay, const char *needle) {
    int i = 0;
    int j;
    if (!hay || !needle || !needle[0])
        return 0;
    while (hay[i]) {
        j = 0;
        while (needle[j] && hay[i + j] == needle[j])
            j++;
        if (!needle[j])
            return 1;
        i++;
    }
    return 0;
}

static void llm_session_reset(void) {
    llm_stream_len = 0;
    llm_stream_buf[0] = '\0';
}

static void llm_session_finish(void) {
    char line[LLM_MAX_RESPONSE + 16];
    int pos = 0;
    int i;

    if (!current_agent || llm_stream_len <= 0)
        return;
    if (agent_has_cap(current_agent, CAP_SVC_LLM))
        return;

    line[pos++] = 'a';
    line[pos++] = 's';
    line[pos++] = 's';
    line[pos++] = 'i';
    line[pos++] = 's';
    line[pos++] = 't';
    line[pos++] = 'a';
    line[pos++] = 'n';
    line[pos++] = 't';
    line[pos++] = ':';
    line[pos++] = ' ';
    for (i = 0; i < llm_stream_len && pos < (int)sizeof(line) - 1; i++)
        line[pos++] = llm_stream_buf[i];
    line[pos] = '\0';
    if (session_append(current_agent, line) >= 0)
        kprintf("[session+llm] appended assistant response\n");
}

static int llm_faux(const char *prompt, char *buf, int buflen) {
    const char *answer = "42";
    int len = 2;

    if (contains_substr(prompt, "17+25"))
        answer = "42";
    else if (contains_substr(prompt, "2+2"))
        answer = "4";
    else if (contains_substr(prompt, "threshold") || contains_substr(prompt, "91"))
        answer = "High temperature anomaly: reading 91 exceeds threshold 80.";
    else
        answer = "ok";

    len = 0;
    while (answer[len])
        len++;

    if (len >= buflen)
        return EINVAL;

    for (int i = 0; i < len; i++) {
        if (agent_has_cap(current_agent, CAP_LOG))
            agent_write(answer + i, 1);
        if (llm_stream_len < LLM_MAX_RESPONSE - 1)
            llm_stream_buf[llm_stream_len++] = answer[i];
        kprintf("[llm] delta len=1\n");
    }

    for (int i = 0; i <= len; i++)
        buf[i] = answer[i];
    return len;
}

static void llm_delta_write(const char *chunk, int len, void *ctx) {
    int i;

    (void)ctx;
    if (current_agent && agent_has_cap(current_agent, CAP_LOG))
        agent_write(chunk, len);
    if (!chunk || len <= 0)
        return;
    for (i = 0; i < len && llm_stream_len < LLM_MAX_RESPONSE - 1; i++)
        llm_stream_buf[llm_stream_len++] = chunk[i];
    kprintf("[llm] delta len=%d\n", len);
}

int agent_llm_query(const char *prompt, char *buf, int buflen) {
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_LLM))
        return EPERM;
    if (!prompt || !buf || buflen <= 0)
        return EINVAL;

    llm_session_reset();
    agent_set_phase(AGENT_PHASE_LLM_WAIT);

#ifdef LLM_FAUX
    rc = llm_faux(prompt, buf, buflen);
#elif defined(LLM_NATIVE_NET)
    if (netstack_ready())
        rc = llm_deepseek_query(prompt, buf, buflen, llm_delta_write, 0);
    else
        rc = ENODEV;
    if (rc == ENODEV)
        rc = llm_faux(prompt, buf, buflen);
#else
    rc = virtio_llm_stream(prompt, buf, buflen, llm_delta_write, 0);
    if (rc == ENODEV)
        rc = llm_faux(prompt, buf, buflen);
#endif

    if (rc >= 0)
        llm_session_finish();

    agent_set_phase(AGENT_PHASE_IDLE);
    return rc;
}
