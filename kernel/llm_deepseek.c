#include "llm_deepseek.h"
#include "llm.h"
#include "netstack.h"
#include "printf.h"
#include "../include/agentos.h"
#include "../generated/deepseek_key.h"
#include "../generated/deepseek_host.h"

#define DEEPSEEK_HOST "api.deepseek.com"
#define DEEPSEEK_PATH "/chat/completions"
#define DEEPSEEK_GW_HOST "10.0.2.2"
#define DEEPSEEK_GW_PORT 8443

static int json_escape(const char *in, char *out, int outlen) {
    int pos = 0;
    int i = 0;

    if (!in || !out || outlen <= 0)
        return 0;
    while (in[i] && pos < outlen - 2) {
        if (in[i] == '"' || in[i] == '\\') {
            if (pos + 2 >= outlen)
                break;
            out[pos++] = '\\';
        }
        out[pos++] = in[i++];
    }
    out[pos] = '\0';
    return pos;
}

static int extract_content(const char *json, char *out, int outlen) {
    const char *key = "\"content\"";
    int i = 0;
    int ki;

    if (!json || !out || outlen <= 0)
        return EINVAL;

    while (json[i]) {
        for (ki = 0; key[ki]; ki++) {
            if (json[i + ki] != key[ki])
                break;
        }
        if (key[ki] == '\0') {
            int op = 0;
            i += ki;
            while (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')
                i++;
            if (json[i] != ':')
                continue;
            i++;
            while (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')
                i++;
            if (json[i] != '"')
                continue;
            i++;
            while (json[i] && json[i] != '"' && op < outlen - 1) {
                if (json[i] == '\\' && json[i + 1]) {
                    i++;
                    if (json[i] == 'n')
                        out[op++] = '\n';
                    else if (json[i] == 't')
                        out[op++] = '\t';
                    else
                        out[op++] = json[i];
                    i++;
                    continue;
                }
                out[op++] = json[i++];
            }
            out[op] = '\0';
            return op;
        }
        i++;
    }
    return EIO;
}

static int api_key_present(void) {
    return DEEPSEEK_API_KEY[0] != '\0';
}

static char deepseek_esc[512];
static char deepseek_body[1024];
static char deepseek_resp[8192];
static char deepseek_content[LLM_MAX_RESPONSE];
static char deepseek_auth[192];

int llm_deepseek_query(const char *prompt, char *buf, int buflen, llm_delta_fn on_delta,
                       void *ctx) {
    int pos = 0;
    int i;
    int clen;
    int blen;

    if (!prompt || !buf || buflen <= 0)
        return EINVAL;
    if (!netstack_ready())
        return ENODEV;
    if (!api_key_present()) {
        kprintf("[deepseek] no API key (set DEEPSEEK_API_KEY in .env)\n");
        return ENODEV;
    }

    json_escape(prompt, deepseek_esc, (int)sizeof(deepseek_esc));
    pos = 0;
    {
        const char *pfx =
            "{\"model\":\"deepseek-chat\",\"messages\":[{\"role\":\"user\",\"content\":\"";
        for (i = 0; pfx[i] && pos < (int)sizeof(deepseek_body) - 64; i++)
            deepseek_body[pos++] = pfx[i];
    }
    for (i = 0; deepseek_esc[i] && pos < (int)sizeof(deepseek_body) - 64; i++)
        deepseek_body[pos++] = deepseek_esc[i];
    {
        const char *sfx = "\"}],\"max_tokens\":256,\"temperature\":0}";
        for (i = 0; sfx[i] && pos < (int)sizeof(deepseek_body) - 4; i++)
            deepseek_body[pos++] = sfx[i];
    }
    deepseek_body[pos] = '\0';

    pos = 0;
    {
        const char *pfx = "Authorization: Bearer ";
        for (i = 0; pfx[i] && pos < (int)sizeof(deepseek_auth) - 96; i++)
            deepseek_auth[pos++] = pfx[i];
    }
    for (i = 0; DEEPSEEK_API_KEY[i] && pos < (int)sizeof(deepseek_auth) - 4; i++)
        deepseek_auth[pos++] = DEEPSEEK_API_KEY[i];
    deepseek_auth[pos] = '\0';

    kprintf("[deepseek] POST https://%s%s\n", DEEPSEEK_HOST, DEEPSEEK_PATH);
    kprintf("[deepseek] resolve baked ip=%s\n", DEEPSEEK_API_IP_STR);
    blen = net_https_post_ip(DEEPSEEK_HOST, DEEPSEEK_API_IP_HOST, DEEPSEEK_PATH, deepseek_auth,
                             deepseek_body, deepseek_resp, (int)sizeof(deepseek_resp), 60000);
    if (blen <= 0) {
        kprintf("[deepseek] native TLS failed (%d), trying host gw http://%s:%u\n", blen,
                DEEPSEEK_GW_HOST, (unsigned)DEEPSEEK_GW_PORT);
        blen = net_http_post(DEEPSEEK_GW_HOST, DEEPSEEK_GW_PORT, DEEPSEEK_PATH, deepseek_auth,
                             deepseek_body, deepseek_resp, (int)sizeof(deepseek_resp), 120000);
    }
    if (blen <= 0)
        return blen < 0 ? blen : EIO;

    clen = extract_content(deepseek_resp, deepseek_content, (int)sizeof(deepseek_content));
    if (clen < 0) {
        for (i = 0; i + 3 < blen; i++) {
            if (deepseek_resp[i] == '\r' && deepseek_resp[i + 1] == '\n' &&
                deepseek_resp[i + 2] == '\r' && deepseek_resp[i + 3] == '\n') {
                clen = extract_content(deepseek_resp + i + 4, deepseek_content,
                                       (int)sizeof(deepseek_content));
                break;
            }
        }
    }
    if (clen < 0)
        return clen;
    if (clen >= buflen)
        return EINVAL;
    for (int j = 0; j <= clen; j++)
        buf[j] = deepseek_content[j];
    if (on_delta && clen > 0)
        on_delta(deepseek_content, clen, ctx);
    kprintf("[deepseek] answer len=%d\n", clen);
    return clen;
}
