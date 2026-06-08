#include "virtio_net.h"
#include "printf.h"
#include "vm.h"
#include "agentos.h"

#define VIRTIO_VENDOR_ID 0x1af4
#define VIRTIO_DEV_NET   0x1041
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define PCI_CAP_VNDR 0x09
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

#define VNET_VQ_SIZE 8
#define VNET_HDR_LEN 10

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

static inline void outl(unsigned short port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(unsigned short port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

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
    uint16_t unused;
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VNET_VQ_SIZE];
    uint16_t unused;
};

struct virtqueue {
    int qidx;
    uint16_t last_used;
    uint16_t notify_off;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

static volatile struct virtio_pci_common_cfg *common_cfg;
static volatile uint16_t *notify_base;
static uint32_t notify_stride;
static volatile uint8_t *device_cfg;

static struct virtqueue net_rxq;
static struct virtqueue net_txq;
static int net_ready;
static uint8_t net_mac[6];

static struct vring_desc rx_desc[VNET_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail rx_avail __attribute__((aligned(4096)));
static struct vring_used rx_used __attribute__((aligned(4096)));

static struct vring_desc tx_desc[VNET_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail tx_avail __attribute__((aligned(4096)));
static struct vring_used tx_used __attribute__((aligned(4096)));

static uint8_t rx_pool[VNET_VQ_SIZE][2048] __attribute__((aligned(16)));
static uint8_t tx_buf[2048] __attribute__((aligned(16)));
static int rx_slots[VNET_VQ_SIZE];

struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t guest_feature_select;
    uint32_t guest_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint32_t queue_desc_lo;
    uint32_t queue_desc_hi;
    uint32_t queue_avail_lo;
    uint32_t queue_avail_hi;
    uint32_t queue_used_lo;
    uint32_t queue_used_hi;
};

static uint32_t pci_read32(int bus, int dev, int func, int off) {
    outl(PCI_CONFIG_ADDR, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                                 ((uint32_t)func << 8) | (uint32_t)(off & 0xfc));
    return inl(PCI_CONFIG_DATA);
}

static void pci_write16(int bus, int dev, int func, int off, uint16_t val) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                    ((uint32_t)func << 8) | (uint32_t)(off & 0xfc);
    uint32_t old;

    outl(PCI_CONFIG_ADDR, addr);
    old = inl(PCI_CONFIG_DATA);
    if ((off & 2) == 0)
        old = (old & 0xffff0000u) | val;
    else
        old = (old & 0x0000ffffu) | ((uint32_t)val << 16);
    outl(PCI_CONFIG_DATA, old);
}

static uint8_t pci_read8(int bus, int dev, int func, int off) {
    uint32_t w = pci_read32(bus, dev, func, off & ~3);
    return (uint8_t)(w >> ((off & 3) * 8));
}

static unsigned long pci_bar_addr(int bus, int dev, int func, int bar) {
    uint32_t raw = pci_read32(bus, dev, func, 0x10 + bar * 4);
    if (raw & 1u)
        return 0;
    if ((raw & 0x6u) == 0x4u)
        return raw & 0xfffffff0UL;
    return raw & 0xfffffff0UL;
}

static int pci_map_bar_mmio(unsigned long pa, unsigned long size) {
    extern pagetable_t kernel_pt;
    return vm_map_region(kernel_pt, pa, pa, size, PTE_R | PTE_W);
}

static int pci_find_net(int *bus, int *dev, int *func) {
    int b, d, f;

    for (b = 0; b < 256; b++) {
        for (d = 0; d < 32; d++) {
            for (f = 0; f < 8; f++) {
                uint32_t id = pci_read32(b, d, f, 0);
                uint16_t vendor = (uint16_t)(id & 0xffff);
                uint16_t device = (uint16_t)(id >> 16);

                if (vendor != VIRTIO_VENDOR_ID)
                    continue;
                if (device != VIRTIO_DEV_NET && device != 0x1000)
                    continue;
                *bus = b;
                *dev = d;
                *func = f;
                return 0;
            }
        }
    }
    return ENODEV;
}

static int pci_map_virtio_caps(int bus, int dev, int func) {
    uint8_t cap = pci_read8(bus, dev, func, 0x34);

    common_cfg = 0;
    notify_base = 0;
    device_cfg = 0;
    while (cap >= 0x40) {
        if (pci_read8(bus, dev, func, cap) == PCI_CAP_VNDR) {
            uint8_t cfg_type = pci_read8(bus, dev, func, cap + 3);
            uint8_t bar = pci_read8(bus, dev, func, cap + 4);
            uint32_t off = pci_read32(bus, dev, func, cap + 8);
            unsigned long bar_pa = pci_bar_addr(bus, dev, func, bar);
            unsigned long mmio;

            if (bar_pa == 0)
                goto next_cap;
            mmio = bar_pa + off;
            if (pci_map_bar_mmio(bar_pa, 4096) != 0)
                goto next_cap;

            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG)
                common_cfg = (volatile struct virtio_pci_common_cfg *)mmio;
            if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                notify_base = (volatile uint16_t *)mmio;
                notify_stride = pci_read32(bus, dev, func, cap + 16);
                if (notify_stride == 0)
                    notify_stride = 4;
            }
            if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG)
                device_cfg = (volatile uint8_t *)mmio;
        }
    next_cap:
        cap = pci_read8(bus, dev, func, cap + 1);
    }

    if (!common_cfg || !notify_base)
        return ENODEV;
    return 0;
}

static void vq_bind(struct virtqueue *vq, int qidx, struct vring_desc *desc,
                    struct vring_avail *avail, struct vring_used *used) {
    vq->qidx = qidx;
    vq->last_used = 0;
    vq->desc = desc;
    vq->avail = avail;
    vq->used = used;
    vq->avail->flags = 0;
    vq->avail->idx = 0;
    vq->used->flags = 0;
    vq->used->idx = 0;
}

static int vq_setup(struct virtqueue *vq) {
    unsigned long desc_pa = (unsigned long)vq->desc;
    unsigned long avail_pa = (unsigned long)vq->avail;
    unsigned long used_pa = (unsigned long)vq->used;
    uint16_t qmax;

    common_cfg->queue_select = (uint16_t)vq->qidx;
    qmax = common_cfg->queue_size;
    if (qmax == 0 || qmax < VNET_VQ_SIZE)
        return EINVAL;
    common_cfg->queue_size = VNET_VQ_SIZE;

    common_cfg->queue_desc_lo = (uint32_t)desc_pa;
    common_cfg->queue_desc_hi = 0;
    common_cfg->queue_avail_lo = (uint32_t)avail_pa;
    common_cfg->queue_avail_hi = 0;
    common_cfg->queue_used_lo = (uint32_t)used_pa;
    common_cfg->queue_used_hi = 0;
    common_cfg->queue_msix_vector = 0xffff;
    vq->notify_off = common_cfg->queue_notify_off;
    common_cfg->queue_enable = 0;
    common_cfg->queue_enable = 1;
    return 0;
}

static void vq_submit(struct virtqueue *vq, int desc_idx) {
    volatile uint16_t *notify_port;
    uint16_t slot = vq->avail->idx % VNET_VQ_SIZE;

    vq->avail->ring[slot] = (uint16_t)desc_idx;
    __asm__ volatile("mfence" ::: "memory");
    vq->avail->idx++;
    __asm__ volatile("mfence" ::: "memory");
    notify_port = (volatile uint16_t *)((unsigned long)notify_base +
                                        (unsigned long)vq->notify_off * notify_stride);
    *notify_port = (uint16_t)vq->qidx;
    __asm__ volatile("mfence" ::: "memory");
}

static int vq_wait_used(struct virtqueue *vq, int timeout_loops) {
    int i;
    for (i = 0; i < timeout_loops; i++) {
        __asm__ volatile("mfence" ::: "memory");
        if (vq->used->idx != vq->last_used)
            return 0;
        __asm__ volatile("pause" ::: "memory");
    }
    return ETIMEDOUT;
}

static void net_read_mac(void) {
    int i;
    if (device_cfg) {
        for (i = 0; i < 6; i++)
            net_mac[i] = device_cfg[i];
    } else {
        net_mac[0] = 0x52;
        net_mac[1] = 0x54;
        net_mac[2] = 0x00;
        net_mac[3] = 0x12;
        net_mac[4] = 0x34;
        net_mac[5] = 0x56;
    }
    kprintf("[virtio-net-pci] mac=%x:%x:%x:%x:%x:%x\n", net_mac[0], net_mac[1],
            net_mac[2], net_mac[3], net_mac[4], net_mac[5]);
}

static void net_rx_refill(int slot) {
    struct vring_desc *d = &net_rxq.desc[slot];

    rx_slots[slot] = slot;
    d->addr = (uint64_t)(unsigned long)rx_pool[slot];
    d->len = (uint32_t)sizeof(rx_pool[slot]);
    d->flags = VRING_DESC_F_WRITE;
    d->next = 0;
    vq_submit(&net_rxq, slot);
}

void virtio_net_init(void) {
    int bus, dev, fn;
    int i;
    uint8_t status;

    if (net_ready)
        return;

    if (pci_find_net(&bus, &dev, &fn) != 0) {
        kprintf("[virtio-net-pci] device not found\n");
        return;
    }

    pci_write16(bus, dev, fn, 4, (uint16_t)(pci_read32(bus, dev, fn, 4) | 0x0007));
    if (pci_map_virtio_caps(bus, dev, fn) != 0) {
        kprintf("[virtio-net-pci] caps missing\n");
        return;
    }

    common_cfg->device_status = 0;
    common_cfg->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER;
    common_cfg->device_feature_select = 0;
    common_cfg->guest_feature_select = 0;
    common_cfg->guest_feature = 0;
    common_cfg->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    status = common_cfg->device_status;
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio-net-pci] feature negotiation failed\n");
        return;
    }

    vq_bind(&net_rxq, 0, rx_desc, &rx_avail, &rx_used);
    vq_bind(&net_txq, 1, tx_desc, &tx_avail, &tx_used);
    if (vq_setup(&net_rxq) != 0 || vq_setup(&net_txq) != 0) {
        kprintf("[virtio-net-pci] queue setup failed\n");
        return;
    }

    net_read_mac();
    for (i = 0; i < VNET_VQ_SIZE; i++)
        net_rx_refill(i);

    common_cfg->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
        VIRTIO_STATUS_DRIVER_OK;
    net_ready = 1;
    kprintf("[virtio-net-pci] driver ready bus=%d dev=%d fn=%d\n", bus, dev, fn);
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
    net_txq.desc[0].addr = (uint64_t)(unsigned long)tx_buf;
    net_txq.desc[0].len = (uint32_t)(len + VNET_HDR_LEN);
    net_txq.desc[0].flags = 0;
    net_txq.desc[0].next = 0;
    vq_submit(&net_txq, 0);

    if (vq_wait_used(&net_txq, 500000) != 0)
        return ETIMEDOUT;
    net_txq.last_used++;
    return len;
}

int virtio_net_poll_frame(void *frame, int frame_cap, int *out_len) {
    uint32_t rx_len;
    int slot;
    int eth_len;

    if (!net_ready || !frame || !out_len || frame_cap <= 0)
        return EINVAL;

    __asm__ volatile("mfence" ::: "memory");
    if (net_rxq.used->idx == net_rxq.last_used)
        return EAGAIN;

    {
        struct vring_used_elem *elem =
            &net_rxq.used->ring[net_rxq.last_used % VNET_VQ_SIZE];
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
}
