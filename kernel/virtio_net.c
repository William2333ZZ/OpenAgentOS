#include "virtio_net.h"
#include "printf.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define VIRTIO_MAGIC       0x74726976
#define VIRTIO_DEV_NET     1

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VRING_DESC_F_NEXT   1
#define VRING_DESC_F_WRITE  2

#define VNET_VQ_SIZE 8
#define VNET_HDR_LEN 10

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VNET_VQ_SIZE];
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VNET_VQ_SIZE];
};

struct virtqueue {
    volatile uint32_t *mmio;
    int qidx;
    uint16_t last_used;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

static volatile uint32_t *net_mmio;
static struct virtqueue net_rxq;
static struct virtqueue net_txq;
static int net_ready;
static uint8_t net_mac[6];

static struct vring_desc rx_desc[VNET_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail rx_avail __attribute__((aligned(4)));
static struct vring_used rx_used __attribute__((aligned(4)));

static struct vring_desc tx_desc[VNET_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail tx_avail __attribute__((aligned(4)));
static struct vring_used tx_used __attribute__((aligned(4)));

static uint8_t rx_pool[VNET_VQ_SIZE][2048] __attribute__((aligned(16)));
static uint8_t tx_buf[2048] __attribute__((aligned(16)));
static int rx_slots[VNET_VQ_SIZE];

static inline uint32_t mmio_read(volatile uint32_t *base, int off) {
    return base[off / 4];
}

static inline void mmio_write(volatile uint32_t *base, int off, uint32_t val) {
    base[off / 4] = val;
}

static inline void fence(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static volatile uint32_t *find_net_mmio(void) {
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
        if (version != 2) {
            kprintf("[virtio-net] slot %d unsupported mmio version %u\n", i, version);
            continue;
        }
        if (devid != VIRTIO_DEV_NET)
            continue;
        kprintf("[virtio-net] slot %d mmio=%lx\n", i, (unsigned long)base);
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
        if (qmax == 0 || qmax < VNET_VQ_SIZE) {
            kprintf("[virtio-net] queue %d max=%u invalid\n", vq->qidx, qmax);
            return;
        }
    }
    mmio_write(vq->mmio, 0x038, VNET_VQ_SIZE);
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
    uint16_t slot = vq->avail->idx & (VNET_VQ_SIZE - 1);
    vq->avail->ring[slot] = (uint16_t)desc_idx;
    fence();
    vq->avail->idx++;
    fence();
    mmio_write(vq->mmio, 0x050, vq->qidx);
}

static void vq_kick(struct virtqueue *vq) {
    mmio_write(vq->mmio, 0x050, vq->qidx);
}

static int vq_wait_used(struct virtqueue *vq, int timeout_loops) {
    for (int i = 0; i < timeout_loops; i++) {
        fence();
        if (vq->used->idx != vq->last_used)
            return 0;
        for (volatile int j = 0; j < 500; j++)
            ;
    }
    return ETIMEDOUT;
}

static int vq_consume(struct virtqueue *vq, uint32_t *out_len) {
    if (vq->used->idx == vq->last_used)
        return EAGAIN;
    {
        struct vring_used_elem *elem =
            &vq->used->ring[vq->last_used & (VNET_VQ_SIZE - 1)];
        if (out_len)
            *out_len = elem->len;
        vq->last_used++;
    }
    return 0;
}

static void net_rx_refill(int slot) {
    rx_desc[slot].addr = (unsigned long)rx_pool[slot];
    rx_desc[slot].len = sizeof(rx_pool[slot]);
    rx_desc[slot].flags = VRING_DESC_F_WRITE;
    rx_desc[slot].next = 0;
    rx_slots[slot] = 1;
    vq_submit(&net_rxq, slot);
}

static void net_read_mac(void) {
    uint32_t lo = mmio_read(net_mmio, 0x100);
    uint32_t hi = mmio_read(net_mmio, 0x104);

    net_mac[0] = (uint8_t)(lo & 0xff);
    net_mac[1] = (uint8_t)((lo >> 8) & 0xff);
    net_mac[2] = (uint8_t)((lo >> 16) & 0xff);
    net_mac[3] = (uint8_t)((lo >> 24) & 0xff);
    net_mac[4] = (uint8_t)(hi & 0xff);
    net_mac[5] = (uint8_t)((hi >> 8) & 0xff);
    kprintf("[virtio-net] mac=%02x:%02x:%02x:%02x:%02x:%02x\n", net_mac[0],
            net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5]);
}

void virtio_net_init(void) {
    int i;

    if (net_ready)
        return;

    net_mmio = find_net_mmio();
    if (!net_mmio) {
        kprintf("[virtio-net] no virtio-net device\n");
        return;
    }

    mmio_write(net_mmio, 0x070, 0);
    fence();
    mmio_write(net_mmio, 0x070, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    fence();
    mmio_write(net_mmio, 0x014, 0);
    mmio_write(net_mmio, 0x014, 1);
    (void)mmio_read(net_mmio, 0x010);
    (void)mmio_read(net_mmio, 0x010);
    mmio_write(net_mmio, 0x024, 0);
    mmio_write(net_mmio, 0x020, 0);
    mmio_write(net_mmio, 0x024, 1);
    mmio_write(net_mmio, 0x020, 0);
    mmio_write(net_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    fence();
    if (!(mmio_read(net_mmio, 0x070) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio-net] feature negotiation failed\n");
        mmio_write(net_mmio, 0x070, 0);
        return;
    }

    net_read_mac();
    vq_bind(&net_rxq, net_mmio, 0, rx_desc, &rx_avail, &rx_used);
    vq_bind(&net_txq, net_mmio, 1, tx_desc, &tx_avail, &tx_used);
    vq_setup(&net_rxq);
    vq_setup(&net_txq);

    for (i = 0; i < VNET_VQ_SIZE; i++)
        net_rx_refill(i);

    mmio_write(net_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);
    fence();
    net_ready = 1;
    kprintf("[virtio-net] driver ready\n");
}

int virtio_net_ready(void) {
    return net_ready;
}

const uint8_t *virtio_net_mac(void) {
    return net_mac;
}

int virtio_net_send_frame(const void *frame, int len) {
    if (!net_ready || !frame || len <= 0 || len + VNET_HDR_LEN > (int)sizeof(tx_buf))
        return EINVAL;

    tx_buf[0] = 0;
    tx_buf[1] = 0;
    for (int i = 0; i < 8; i++)
        tx_buf[2 + i] = 0;
    for (int i = 0; i < len; i++)
        tx_buf[VNET_HDR_LEN + i] = ((const uint8_t *)frame)[i];

    net_txq.last_used = net_txq.used->idx;
    tx_desc[0].addr = (unsigned long)tx_buf;
    tx_desc[0].len = (uint32_t)(len + VNET_HDR_LEN);
    tx_desc[0].flags = 0;
    tx_desc[0].next = 0;
    vq_submit(&net_txq, 0);

    if (vq_wait_used(&net_txq, 500000) != 0)
        return ETIMEDOUT;
    vq_consume(&net_txq, 0);
    return len;
}

int virtio_net_poll_frame(void *frame, int frame_cap, int *out_len) {
    uint32_t rx_len;
    int slot;
    int eth_len;

    if (!net_ready || !frame || !out_len || frame_cap <= 0)
        return EINVAL;

    fence();
    if (net_rxq.used->idx == net_rxq.last_used)
        return EAGAIN;

    {
        struct vring_used_elem *elem =
            &net_rxq.used->ring[net_rxq.last_used & (VNET_VQ_SIZE - 1)];
        slot = (int)elem->id;
        rx_len = elem->len;
        net_rxq.last_used++;
    }

    if (slot < 0 || slot >= VNET_VQ_SIZE)
        return EIO;
    if (rx_len <= VNET_HDR_LEN)
        return EIO;

    eth_len = (int)rx_len - VNET_HDR_LEN;
    if (eth_len > frame_cap)
        eth_len = frame_cap;
    for (int i = 0; i < eth_len; i++)
        ((uint8_t *)frame)[i] = rx_pool[slot][VNET_HDR_LEN + i];
    *out_len = eth_len;

    net_rx_refill(slot);
    return 0;
}

void virtio_net_poll(void) {
    (void)0;
}
