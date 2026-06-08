#include "virtio_input.h"
#include "printf.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define VIRTIO_MAGIC      0x74726976
#define VIRTIO_DEV_INPUT  18

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VRING_DESC_F_WRITE 2

#define INPUT_VQ_SIZE 16
#define INPUT_CFG_EV_BITS 0x03

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[INPUT_VQ_SIZE];
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[INPUT_VQ_SIZE];
};

struct virtqueue {
    volatile uint32_t *mmio;
    int qidx;
    uint16_t last_used;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
};

static volatile uint32_t *input_mmio;
static struct virtqueue input_evq;
static int input_ready;

static struct vring_desc ev_desc[INPUT_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail ev_avail __attribute__((aligned(4)));
static struct vring_used ev_used __attribute__((aligned(4)));
static struct virtio_input_event ev_buf[INPUT_VQ_SIZE] __attribute__((aligned(16)));
static int ev_slots[INPUT_VQ_SIZE];

static inline uint32_t mmio_read(volatile uint32_t *base, int off) {
    return base[off / 4];
}

static inline void mmio_write(volatile uint32_t *base, int off, uint32_t val) {
    base[off / 4] = val;
}

static inline void fence(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static void cfg_write8(volatile uint32_t *mmio, int offset, uint8_t val) {
    uint32_t cur = mmio_read(mmio, 0x100 + (offset & ~3));
    int shift = (offset & 3) * 8;
    cur &= ~(0xffu << shift);
    cur |= ((uint32_t)val << shift);
    mmio_write(mmio, 0x100 + (offset & ~3), cur);
}

static void input_select_ev_bits(int subsel) {
    uint8_t bits[128];
    int i;

    cfg_write8(input_mmio, 0, INPUT_CFG_EV_BITS);
    cfg_write8(input_mmio, 1, (uint8_t)subsel);
    cfg_write8(input_mmio, 2, 128);
    cfg_write8(input_mmio, 3, 0);
    for (i = 0; i < 128; i++)
        bits[i] = 0xff;
    for (i = 0; i < 128; i++)
        cfg_write8(input_mmio, 8 + i, bits[i]);
}

static volatile uint32_t *find_input_mmio(void) {
    int count = platform_virtio_mmio_count();
    unsigned long base_addr = platform_virtio_mmio_base();
    unsigned long stride = platform_virtio_mmio_stride();
    int i;

    for (i = 0; i < count; i++) {
        volatile uint32_t *base =
            (volatile uint32_t *)(base_addr + (unsigned long)i * stride);
        uint32_t magic = mmio_read(base, 0x000);
        uint32_t version;
        uint32_t devid;

        if (magic != VIRTIO_MAGIC)
            continue;
        version = mmio_read(base, 0x004);
        devid = mmio_read(base, 0x008);
        if (version != 2)
            continue;
        if (devid != VIRTIO_DEV_INPUT)
            continue;
        kprintf("[virtio-input] slot %d mmio=%lx\n", i, (unsigned long)base);
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
    {
        uint32_t qmax = mmio_read(vq->mmio, 0x034);
        if (qmax == 0 || qmax < INPUT_VQ_SIZE) {
            kprintf("[virtio-input] queue %d max=%u invalid\n", vq->qidx, qmax);
            return;
        }
    }
    mmio_write(vq->mmio, 0x038, INPUT_VQ_SIZE);
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
    uint16_t slot = vq->avail->idx & (INPUT_VQ_SIZE - 1);
    vq->avail->ring[slot] = (uint16_t)desc_idx;
    fence();
    vq->avail->idx++;
    fence();
    mmio_write(vq->mmio, 0x050, vq->qidx);
}

static void input_refill(int slot) {
    ev_desc[slot].addr = (unsigned long)&ev_buf[slot];
    ev_desc[slot].len = sizeof(ev_buf[slot]);
    ev_desc[slot].flags = VRING_DESC_F_WRITE;
    ev_desc[slot].next = 0;
    ev_slots[slot] = 1;
    vq_submit(&input_evq, slot);
}

void virtio_input_init(void) {
    int i;

    if (input_ready)
        return;

    input_mmio = find_input_mmio();
    if (!input_mmio) {
        kprintf("[virtio-input] no virtio-input device\n");
        return;
    }

    mmio_write(input_mmio, 0x070, 0);
    fence();
    mmio_write(input_mmio, 0x070, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    fence();
    mmio_write(input_mmio, 0x014, 0);
    mmio_write(input_mmio, 0x014, 1);
    (void)mmio_read(input_mmio, 0x010);
    (void)mmio_read(input_mmio, 0x010);
    mmio_write(input_mmio, 0x024, 0);
    mmio_write(input_mmio, 0x020, 0);
    mmio_write(input_mmio, 0x024, 1);
    mmio_write(input_mmio, 0x020, 0);
    mmio_write(input_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    fence();
    if (!(mmio_read(input_mmio, 0x070) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio-input] feature negotiation failed\n");
        mmio_write(input_mmio, 0x070, 0);
        return;
    }

    input_select_ev_bits(0);
    input_select_ev_bits(1);

    vq_bind(&input_evq, input_mmio, 0, ev_desc, &ev_avail, &ev_used);
    vq_setup(&input_evq);
    for (i = 0; i < INPUT_VQ_SIZE; i++)
        input_refill(i);

    mmio_write(input_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);
    fence();
    input_ready = 1;
    kprintf("[virtio-input] driver ready\n");
}

int virtio_input_ready(void) {
    return input_ready;
}

int virtio_input_poll(int *type, int *code, int *value) {
    struct vring_used_elem *elem;
    int slot;
    struct virtio_input_event ev;
    int tries;

    if (!input_ready)
        return ENODEV;

    for (tries = 0; tries < 4; tries++) {
        fence();
        if (input_evq.used->idx != input_evq.last_used)
            break;
        for (volatile int j = 0; j < 500; j++)
            ;
    }
    if (input_evq.used->idx == input_evq.last_used)
        return 0;

    elem = &input_evq.used->ring[input_evq.last_used & (INPUT_VQ_SIZE - 1)];
    slot = (int)elem->id;
    input_evq.last_used++;
    if (slot < 0 || slot >= INPUT_VQ_SIZE)
        return EIO;

    ev = ev_buf[slot];
    input_refill(slot);

    if (type)
        *type = ev.type;
    if (code)
        *code = ev.code;
    if (value)
        *value = (int)ev.value;
    return 1;
}
