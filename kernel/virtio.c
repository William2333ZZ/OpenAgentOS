#include "virtio.h"
#include "printf.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define VIRTIO_MAGIC       0x74726976
#define VIRTIO_DEV_CONSOLE 3
#define VRING_DESC_F_WRITE 2

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VQ_SIZE 4

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VQ_SIZE];
    uint16_t used_event;
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VQ_SIZE];
    uint16_t avail_event;
};

struct virtqueue {
    volatile uint32_t *mmio;
    int qidx;
    uint16_t last_used;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

static volatile uint32_t *console_mmio;
static struct virtqueue rxq;
static struct virtqueue txq;
static int virtio_ready;

static struct vring_desc rx_desc[VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail rx_avail __attribute__((aligned(4)));
static struct vring_used rx_used __attribute__((aligned(4)));

static struct vring_desc tx_desc[VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail tx_avail __attribute__((aligned(4)));
static struct vring_used tx_used __attribute__((aligned(4)));

static char rx_buf[LLM_MAX_RESPONSE + 64];
static char tx_buf[LLM_MAX_PROMPT + 64];

static inline uint32_t mmio_read(volatile uint32_t *base, int off) {
    return base[off / 4];
}

static inline void mmio_write(volatile uint32_t *base, int off, uint32_t val) {
    base[off / 4] = val;
}

static inline void fence(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static volatile uint32_t *find_console_mmio(void) {
    int count = platform_virtio_mmio_count();
    unsigned long base_addr = platform_virtio_mmio_base();
    unsigned long stride = platform_virtio_mmio_stride();
    int i;

    for (i = 0; i < count; i++) {
        volatile uint32_t *base =
            (volatile uint32_t *)(base_addr + (unsigned long)i * stride);
        uint32_t magic = mmio_read(base, 0x000);
        if (magic != VIRTIO_MAGIC)
            continue;

        uint32_t version = mmio_read(base, 0x004);
        uint32_t devid = mmio_read(base, 0x008);
        kprintf("[virtio] slot %d id=%u ver=%u\n", i, devid, version);
        if (version != 2) {
            kprintf("[virtio] unsupported mmio version %u (use QEMU -global virtio-mmio.force-legacy=false)\n",
                    version);
            return 0;
        }
        if (devid != VIRTIO_DEV_CONSOLE)
            continue;
        return base;
    }
    return 0;
}

static void vq_bind(struct virtqueue *vq, volatile uint32_t *mmio, int qidx,
                    struct vring_desc *desc, struct vring_avail *avail,
                    struct vring_used *used) {
    vq->mmio = mmio;
    vq->qidx = qidx;
    vq->last_used = 0;
    vq->desc = desc;
    vq->avail = avail;
    vq->used = used;
    avail->flags = 0;
    avail->idx = 0;
    used->flags = 0;
    used->idx = 0;
}

static void vq_setup(struct virtqueue *vq) {
    mmio_write(vq->mmio, 0x030, vq->qidx);
    uint32_t qmax = mmio_read(vq->mmio, 0x034);
    if (qmax == 0 || qmax < VQ_SIZE) {
        kprintf("[virtio] queue %d max=%u invalid\n", vq->qidx, qmax);
        return;
    }
    mmio_write(vq->mmio, 0x038, VQ_SIZE);
    mmio_write(vq->mmio, 0x080, (uint32_t)(unsigned long)vq->desc);
    mmio_write(vq->mmio, 0x084, (uint32_t)((unsigned long)vq->desc >> 32));
    mmio_write(vq->mmio, 0x090, (uint32_t)(unsigned long)vq->avail);
    mmio_write(vq->mmio, 0x094, (uint32_t)((unsigned long)vq->avail >> 32));
    mmio_write(vq->mmio, 0x0a0, (uint32_t)(unsigned long)vq->used);
    mmio_write(vq->mmio, 0x0a4, (uint32_t)((unsigned long)vq->used >> 32));
    fence();
    mmio_write(vq->mmio, 0x044, 1);
}

static void vq_submit(struct virtqueue *vq, int desc_idx) {
    uint16_t slot = vq->avail->idx & (VQ_SIZE - 1);
    vq->avail->ring[slot] = desc_idx;
    fence();
    vq->avail->idx++;
    fence();
    mmio_write(vq->mmio, 0x050, vq->qidx);
}

static int vq_wait_used(struct virtqueue *vq, int timeout_loops, const char *name) {
    for (int i = 0; i < timeout_loops; i++) {
        fence();
        if (vq->used->idx != vq->last_used)
            return 0;
        for (volatile int j = 0; j < 500; j++)
            ;
    }
    kprintf("[virtio] timeout waiting %s queue\n", name);
    return ETIMEDOUT;
}

static int vq_consume(struct virtqueue *vq, uint32_t *out_len) {
    if (vq->used->idx == vq->last_used)
        return EAGAIN;
    struct vring_used_elem *elem = &vq->used->ring[vq->last_used & (VQ_SIZE - 1)];
    if (out_len)
        *out_len = elem->len;
    vq->last_used++;
    return 0;
}

int virtio_console_ready(void) {
    return virtio_ready;
}

static int negotiate_features(volatile uint32_t *mmio) {
    mmio_write(mmio, 0x014, 0);
    mmio_write(mmio, 0x014, 1);
    (void)mmio_read(mmio, 0x010);
    (void)mmio_read(mmio, 0x010);
    mmio_write(mmio, 0x024, 0);
    mmio_write(mmio, 0x020, 0);
    mmio_write(mmio, 0x024, 1);
    mmio_write(mmio, 0x020, 0);
    return 0;
}

int virtio_console_init(void) {
    if (virtio_ready)
        return 0;

    console_mmio = find_console_mmio();
    if (!console_mmio)
        return ENODEV;

    kprintf("[virtio] resetting device\n");
    mmio_write(console_mmio, 0x070, 0);
    fence();
    mmio_write(console_mmio, 0x070, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    fence();
    negotiate_features(console_mmio);
    mmio_write(console_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    fence();
    if (!(mmio_read(console_mmio, 0x070) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio] feature negotiation failed\n");
        mmio_write(console_mmio, 0x070, 0);
        return EIO;
    }

    kprintf("[virtio] binding queues\n");
    vq_bind(&rxq, console_mmio, 0, rx_desc, &rx_avail, &rx_used);
    vq_bind(&txq, console_mmio, 1, tx_desc, &tx_avail, &tx_used);
    vq_setup(&rxq);
    kprintf("[virtio] rx queue ready\n");
    vq_setup(&txq);
    kprintf("[virtio] tx queue ready\n");

    mmio_write(console_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);
    fence();

    virtio_ready = 1;
    kprintf("[virtio] console mmio=%x ready\n", (unsigned)(unsigned long)console_mmio);
    return 0;
}

static void put_u32_be(char *p, uint32_t v) {
    p[0] = (char)(v >> 24);
    p[1] = (char)(v >> 16);
    p[2] = (char)(v >> 8);
    p[3] = (char)v;
}

static uint32_t get_u32_be(const char *p) {
    return ((uint32_t)(unsigned char)p[0] << 24) |
           ((uint32_t)(unsigned char)p[1] << 16) |
           ((uint32_t)(unsigned char)p[2] << 8) |
           (uint32_t)(unsigned char)p[3];
}

int virtio_llm_exchange(const char *prompt, char *response, int response_cap) {
    if (!virtio_ready) {
        int rc = virtio_console_init();
        if (rc != 0)
            return rc;
    }
    if (!prompt || !response || response_cap <= 0)
        return EINVAL;

    int plen = 0;
    while (prompt[plen] && plen < LLM_MAX_PROMPT - 1)
        plen++;

    tx_buf[0] = 'L';
    tx_buf[1] = 'L';
    tx_buf[2] = 'M';
    tx_buf[3] = 'Q';
    put_u32_be(tx_buf + 4, (uint32_t)plen);
    for (int i = 0; i < plen; i++)
        tx_buf[8 + i] = prompt[i];
    int tx_len = 8 + plen;

    rxq.last_used = rxq.used->idx;
    txq.last_used = txq.used->idx;

    rx_desc[0].addr = (unsigned long)rx_buf;
    rx_desc[0].len = sizeof(rx_buf);
    rx_desc[0].flags = VRING_DESC_F_WRITE;
    rx_desc[0].next = 0;
    vq_submit(&rxq, 0);

    tx_desc[0].addr = (unsigned long)tx_buf;
    tx_desc[0].len = (uint32_t)tx_len;
    tx_desc[0].flags = 0;
    tx_desc[0].next = 0;
    vq_submit(&txq, 0);

    kprintf("[virtio] llm request sent, waiting host ...\n");

    if (vq_wait_used(&txq, 300000, "tx") != 0)
        return ETIMEDOUT;
    vq_consume(&txq, 0);

    if (vq_wait_used(&rxq, 3000000, "rx") != 0)
        return ETIMEDOUT;

    uint32_t rx_len = 0;
    vq_consume(&rxq, &rx_len);
    if (rx_len < 8)
        return EIO;

    if (rx_buf[0] != 'L' || rx_buf[1] != 'L' || rx_buf[2] != 'M' || rx_buf[3] != 'R')
        return EIO;

    uint32_t body_len = get_u32_be(rx_buf + 4);
    if (body_len + 8 > rx_len || (int)body_len >= response_cap)
        return EIO;

    for (uint32_t i = 0; i < body_len; i++)
        response[i] = rx_buf[8 + i];
    response[body_len] = '\0';
    kprintf("[virtio] llm response len=%u\n", body_len);
    return (int)body_len;
}

static int json_append_str(char *dst, int cap, int *pos, const char *src) {
    if (*pos >= cap - 2)
        return EINVAL;
    dst[(*pos)++] = '"';
    while (*src && *pos < cap - 3) {
        if (*src == '"' || *src == '\\') {
            if (*pos >= cap - 4)
                break;
            dst[(*pos)++] = '\\';
        }
        dst[(*pos)++] = *src++;
    }
    if (*pos >= cap - 2)
        return EINVAL;
    dst[(*pos)++] = '"';
    return 0;
}

static int build_json_query(const char *prompt, char *out, int cap) {
    int pos = 0;
    const char *prefix = "{\"v\":2,\"op\":\"query\",\"prompt\":";
    for (int i = 0; prefix[i]; i++)
        out[pos++] = prefix[i];
    if (json_append_str(out, cap, &pos, prompt) != 0)
        return EINVAL;
    if (pos + 2 >= cap)
        return EINVAL;
    out[pos++] = '}';
    out[pos++] = '\n';
    out[pos] = '\0';
    return pos;
}

static const char *json_field(const char *line, const char *key) {
    char pat[32];
    int i = 0;
    int k = 0;
    const char *r;

    pat[i++] = '"';
    while (key[k] && i < (int)sizeof(pat) - 2)
        pat[i++] = key[k++];
    pat[i++] = '"';
    pat[i] = '\0';

    for (const char *p = line; *p; p++) {
        const char *q = pat;
        r = p;
        while (*q && *r && *q == *r) {
            q++;
            r++;
        }
        if (*q != '\0')
            continue;
        while (*r == ' ' || *r == '\t')
            r++;
        if (*r != ':')
            continue;
        r++;
        while (*r == ' ' || *r == '\t')
            r++;
        if (*r != '"')
            continue;
        return r + 1;
    }
    return 0;
}

static int copy_json_value(const char *src, char *out, int cap) {
    int i = 0;
    if (!src)
        return 0;
    while (*src && *src != '"' && i < cap - 1) {
        if (*src == '\\' && src[1]) {
            out[i++] = src[1];
            src += 2;
            continue;
        }
        out[i++] = *src++;
    }
    out[i] = '\0';
    return i;
}

static int handle_json_line(const char *line, char *response, int response_cap,
                            int *out_len, llm_delta_fn on_delta, void *ctx) {
    char op[16];
    char chunk[128];
    const char *val;

    val = json_field(line, "op");
    if (!val || copy_json_value(val, op, sizeof(op)) <= 0)
        return 0;

    if (op[0] == 'd' && op[1] == 'e' && op[2] == 'l' && op[3] == 't' &&
        op[4] == 'a' && op[5] == '\0') {
        val = json_field(line, "chunk");
        if (val && copy_json_value(val, chunk, sizeof(chunk)) > 0 && on_delta) {
            int clen = 0;
            while (chunk[clen])
                clen++;
            on_delta(chunk, clen, ctx);
            if (*out_len + clen < response_cap - 1) {
                for (int i = 0; i < clen; i++)
                    response[*out_len + i] = chunk[i];
                *out_len += clen;
                response[*out_len] = '\0';
            }
        }
        return 0;
    }

    if (op[0] == 'e' && op[1] == 'n' && op[2] == 'd' && op[3] == '\0') {
        val = json_field(line, "text");
        if (val && copy_json_value(val, chunk, sizeof(chunk)) > 0 && chunk[0]) {
            int clen = 0;
            while (chunk[clen])
                clen++;
            if (clen < response_cap) {
                for (int i = 0; i <= clen; i++)
                    response[i] = chunk[i];
                *out_len = clen;
            }
        }
        return 1;
    }

    if (op[0] == 'e' && op[1] == 'r' && op[2] == 'r' && op[3] == 'o' &&
        op[4] == 'r' && op[5] == '\0')
        return -1;
    return 0;
}

static int parse_legacy_response(char *response, int response_cap, uint32_t rx_len) {
    uint32_t body_len;

    if (rx_len < 8)
        return EIO;
    if (rx_buf[0] != 'L' || rx_buf[1] != 'L' || rx_buf[2] != 'M' || rx_buf[3] != 'R')
        return EIO;
    body_len = get_u32_be(rx_buf + 4);
    if (body_len + 8 > rx_len || (int)body_len >= response_cap)
        return EIO;
    for (uint32_t i = 0; i < body_len; i++)
        response[i] = rx_buf[8 + i];
    response[body_len] = '\0';
    return (int)body_len;
}

int virtio_llm_stream(const char *prompt, char *response, int response_cap,
                      llm_delta_fn on_delta, void *ctx) {
    int tx_len;
    int out_len = 0;
    int line_start = 0;
    int done = 0;

    if (!virtio_ready) {
        int rc = virtio_console_init();
        if (rc != 0)
            return rc;
    }
    if (!prompt || !response || response_cap <= 0)
        return EINVAL;

    response[0] = '\0';
    tx_len = build_json_query(prompt, tx_buf, (int)sizeof(tx_buf));
    if (tx_len < 0)
        return EINVAL;

    rxq.last_used = rxq.used->idx;
    txq.last_used = txq.used->idx;

    rx_desc[0].addr = (unsigned long)rx_buf;
    rx_desc[0].len = sizeof(rx_buf);
    rx_desc[0].flags = VRING_DESC_F_WRITE;
    rx_desc[0].next = 0;
    vq_submit(&rxq, 0);

    tx_desc[0].addr = (unsigned long)tx_buf;
    tx_desc[0].len = (uint32_t)tx_len;
    tx_desc[0].flags = 0;
    tx_desc[0].next = 0;
    vq_submit(&txq, 0);

    kprintf("[virtio] llm stream request sent\n");

    if (vq_wait_used(&txq, 300000, "tx") != 0)
        return ETIMEDOUT;
    vq_consume(&txq, 0);

    if (vq_wait_used(&rxq, 3000000, "rx") != 0)
        return ETIMEDOUT;

    uint32_t rx_len = 0;
    vq_consume(&rxq, &rx_len);
    if (rx_len < 8)
        return EIO;

    if (rx_buf[0] == 'L' && rx_buf[1] == 'L' && rx_buf[2] == 'M' &&
        rx_buf[3] == 'R')
        return parse_legacy_response(response, response_cap, rx_len);

    for (uint32_t i = 0; i < rx_len; i++) {
        if (rx_buf[i] == '\n' || i + 1 == rx_len) {
            char line[256];
            int len = (int)i - line_start;
            if (i + 1 == rx_len && rx_buf[i] != '\n')
                len++;
            if (len <= 0 || len >= (int)sizeof(line)) {
                line_start = (int)i + 1;
                continue;
            }
            for (int j = 0; j < len; j++)
                line[j] = rx_buf[line_start + j];
            line[len] = '\0';
            int rc = handle_json_line(line, response, response_cap, &out_len,
                                      on_delta, ctx);
            if (rc < 0)
                return EIO;
            if (rc > 0) {
                done = 1;
                break;
            }
            line_start = (int)i + 1;
        }
    }

    if (!done && out_len > 0)
        return out_len;
    if (done)
        return out_len > 0 ? out_len : EIO;
    return EIO;
}

int virtio_http_exchange(const char *tx, unsigned long tx_len, char *rx,
                         unsigned long rx_cap) {
    if (!virtio_ready) {
        int rc = virtio_console_init();
        if (rc != 0)
            return rc;
    }
    if (!tx || !rx || tx_len == 0 || rx_cap < 8)
        return EINVAL;
    if (tx_len > sizeof(tx_buf) || rx_cap > sizeof(rx_buf))
        return EINVAL;

    for (unsigned long i = 0; i < tx_len; i++)
        tx_buf[i] = tx[i];

    rxq.last_used = rxq.used->idx;
    txq.last_used = txq.used->idx;

    rx_desc[0].addr = (unsigned long)rx_buf;
    rx_desc[0].len = sizeof(rx_buf);
    rx_desc[0].flags = VRING_DESC_F_WRITE;
    rx_desc[0].next = 0;
    vq_submit(&rxq, 0);

    tx_desc[0].addr = (unsigned long)tx_buf;
    tx_desc[0].len = (uint32_t)tx_len;
    tx_desc[0].flags = 0;
    tx_desc[0].next = 0;
    vq_submit(&txq, 0);

    kprintf("[virtio] http request sent\n");

    if (vq_wait_used(&txq, 300000, "tx") != 0)
        return ETIMEDOUT;
    vq_consume(&txq, 0);

    if (vq_wait_used(&rxq, 3000000, "rx") != 0)
        return ETIMEDOUT;

    {
        uint32_t rx_len = 0;
        vq_consume(&rxq, &rx_len);
        if (rx_len < 8 || rx_len > rx_cap)
            return EIO;
        for (uint32_t i = 0; i < rx_len; i++)
            rx[i] = rx_buf[i];
        kprintf("[virtio] http response len=%u\n", rx_len);
        return (int)rx_len;
    }
}
