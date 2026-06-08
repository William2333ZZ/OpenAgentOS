#include "virtio_blk.h"
#include "printf.h"
#include "vm.h"
#include "../include/agentos.h"

#define VIRTIO_VENDOR_ID 0x1af4
#define VIRTIO_DEV_BLK   0x1001
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

#define VQ_SIZE 256

#define PCI_CAP_VNDR 0x09
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

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
    uint16_t unused;
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VQ_SIZE];
    uint16_t unused;
};

struct virtio_pci_cap {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed));

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
} __attribute__((packed));

struct virtqueue {
    int qidx;
    uint16_t last_used;
    uint16_t notify_off;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

static int pci_bus = -1;
static int pci_dev = -1;
static int pci_fn = -1;
static volatile struct virtio_pci_common_cfg *common_cfg;
static volatile uint16_t *notify_base;
static unsigned notify_stride = 4;
static struct virtqueue blkq;
static int blk_ready;

static uint8_t blk_vring[VQ_SIZE * sizeof(struct vring_desc) + 4096 + 8192]
    __attribute__((aligned(4096)));

static struct virtio_blk_outhdr blk_outhdr;
static uint8_t blk_status;

static inline void outl(unsigned short port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(unsigned short port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(unsigned short port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

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
    if (raw & 1)
        return raw & 0xfffffffcUL;
    return raw & 0xfffffff0UL;
}

static int pci_map_bar_mmio(unsigned long pa, unsigned long size) {
    extern pagetable_t kernel_pt;
    return vm_map_region(kernel_pt, pa, pa, size, PTE_R | PTE_W);
}

static int pci_find_virtio(int *bus, int *dev, int *func) {
    int b, d, f;

    for (b = 0; b < 256; b++) {
        for (d = 0; d < 32; d++) {
            for (f = 0; f < 8; f++) {
                uint32_t id = pci_read32(b, d, f, 0);
                uint16_t vendor = (uint16_t)(id & 0xffff);
                uint16_t device = (uint16_t)(id >> 16);

                if (vendor != VIRTIO_VENDOR_ID)
                    continue;
                if (device != VIRTIO_DEV_BLK && device != 0x1002)
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

            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG)
                common_cfg = (volatile struct virtio_pci_common_cfg *)mmio;
            if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                notify_base = (volatile uint16_t *)mmio;
                notify_stride = pci_read32(bus, dev, func, cap + 16);
                if (notify_stride == 0)
                    notify_stride = 4;
            }
        }
    next_cap:
        cap = pci_read8(bus, dev, func, cap + 1);
    }

    if (!common_cfg || !notify_base)
        return ENODEV;
    return 0;
}

static void vq_bind(struct virtqueue *vq, int qidx) {
    unsigned long used_off;

    vq->qidx = qidx;
    vq->last_used = 0;
    vq->desc = (struct vring_desc *)blk_vring;
    vq->avail = (struct vring_avail *)(blk_vring + sizeof(struct vring_desc) * VQ_SIZE);
    used_off = ((sizeof(struct vring_desc) * VQ_SIZE + sizeof(uint16_t) * (3 + VQ_SIZE) +
                 4095) &
                ~4095UL);
    vq->used = (struct vring_used *)(blk_vring + used_off);
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
    if (qmax == 0 || qmax < VQ_SIZE)
        return EINVAL;

    common_cfg->queue_desc_lo = (uint32_t)desc_pa;
    common_cfg->queue_desc_hi = 0;
    common_cfg->queue_avail_lo = (uint32_t)avail_pa;
    common_cfg->queue_avail_hi = 0;
    common_cfg->queue_used_lo = (uint32_t)used_pa;
    common_cfg->queue_used_hi = 0;
    common_cfg->queue_msix_vector = 0xffff;
    vq->notify_off = common_cfg->queue_notify_off;
    common_cfg->queue_enable = 1;
    return 0;
}

static void vq_submit(struct virtqueue *vq, int desc_idx) {
    volatile uint16_t *notify_port;
    uint16_t slot = vq->avail->idx % VQ_SIZE;

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

static int vq_consume(struct virtqueue *vq) {
    if (vq->used->idx == vq->last_used)
        return EAGAIN;
    vq->last_used++;
    return 0;
}

int virtio_blk_ready(void) {
    return blk_ready;
}

int virtio_blk_init(void) {
    uint32_t features;
    uint8_t status;

    if (blk_ready)
        return 0;

    if (pci_find_virtio(&pci_bus, &pci_dev, &pci_fn) != 0) {
        kprintf("[virtio-blk-pci] device not found\n");
        return ENODEV;
    }

    pci_write16(pci_bus, pci_dev, pci_fn, 4,
                (uint16_t)pci_read32(pci_bus, pci_dev, pci_fn, 4) | 0x0007);

    if (pci_map_virtio_caps(pci_bus, pci_dev, pci_fn) != 0) {
        kprintf("[virtio-blk-pci] modern caps missing\n");
        return ENODEV;
    }

    common_cfg->device_status = 0;
    common_cfg->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER;

    common_cfg->device_feature_select = 0;
    features = common_cfg->device_feature;
    common_cfg->guest_feature_select = 0;
    common_cfg->guest_feature = features & ~((1u << 28) | (1u << 29));
    common_cfg->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    status = common_cfg->device_status;
    if (!(status & VIRTIO_STATUS_FEATURES_OK))
        return EIO;

    vq_bind(&blkq, 0);
    if (vq_setup(&blkq) != 0)
        return EIO;

    common_cfg->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
        VIRTIO_STATUS_DRIVER_OK;
    blk_ready = 1;
    kprintf("[virtio-blk-pci] modern ready bus=%d dev=%d fn=%d qmax=%u\n", pci_bus,
            pci_dev, pci_fn, (unsigned int)common_cfg->queue_size);
    return 0;
}

static int blk_transfer(unsigned long sector, void *buf, int write) {
    struct vring_desc *d;
    int head = 0;

    if (!blk_ready)
        return ENODEV;

    blk_outhdr.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    blk_outhdr.reserved = 0;
    blk_outhdr.sector = sector;
    blk_status = 0xff;

    d = &blkq.desc[0];
    d[0].addr = (uint64_t)(unsigned long)&blk_outhdr;
    d[0].len = sizeof(blk_outhdr);
    d[0].flags = VRING_DESC_F_NEXT;
    d[0].next = 1;

    d[1].addr = (uint64_t)(unsigned long)buf;
    d[1].len = VIRTIO_SECTOR_SIZE;
    d[1].flags = write ? 0 : VRING_DESC_F_WRITE;
    d[1].flags |= VRING_DESC_F_NEXT;
    d[1].next = 2;

    d[2].addr = (uint64_t)(unsigned long)&blk_status;
    d[2].len = 1;
    d[2].flags = VRING_DESC_F_WRITE;
    d[2].next = 0;

    vq_submit(&blkq, head);
    if (vq_wait_used(&blkq, 10000000) != 0)
        return ETIMEDOUT;
    if (vq_consume(&blkq) != 0)
        return EIO;
    if (blk_status != 0)
        return EIO;
    return 0;
}

int virtio_blk_read_sector(unsigned long sector, void *buf) {
    return blk_transfer(sector, buf, 0);
}

int virtio_blk_write_sector(unsigned long sector, const void *buf) {
    return blk_transfer(sector, (void *)buf, 1);
}
