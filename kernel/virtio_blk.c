#include "virtio_blk.h"
#include "printf.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define VIRTIO_MAGIC       0x74726976
#define VIRTIO_DEV_BLK     2
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

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

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

static volatile uint32_t *blk_mmio;
static struct virtqueue blkq;
static int blk_ready;

static struct vring_desc blk_desc[VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail blk_avail __attribute__((aligned(4)));
static struct vring_used blk_used __attribute__((aligned(4)));

static struct virtio_blk_outhdr blk_outhdr;
static char blk_data[VIRTIO_SECTOR_SIZE];
static uint8_t blk_status;

static inline uint32_t mmio_read(volatile uint32_t *base, int off) {
    return base[off / 4];
}

static inline void mmio_write(volatile uint32_t *base, int off, uint32_t val) {
    base[off / 4] = val;
}

static inline void fence(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static volatile uint32_t *find_blk_mmio(void) {
    unsigned long base = platform_virtio_mmio_base();
    int count = platform_virtio_mmio_count();
    unsigned long stride = platform_virtio_mmio_stride();
    int i;

    for (i = 0; i < count; i++) {
        volatile uint32_t *mmio_base =
            (volatile uint32_t *)(base + (unsigned long)i * stride);
        uint32_t magic = mmio_read(mmio_base, 0x000);
        uint32_t version;
        uint32_t devid;

        if (magic != VIRTIO_MAGIC)
            continue;

        version = mmio_read(mmio_base, 0x004);
        devid = mmio_read(mmio_base, 0x008);
        kprintf("[virtio-blk] slot %d id=%u ver=%u\n", i, devid, version);
        if (version != 2) {
            kprintf("[virtio-blk] need mmio v2 (QEMU -global virtio-mmio.force-legacy=false)\n");
            continue;
        }
        if (devid != VIRTIO_DEV_BLK)
            continue;
        return mmio_base;
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

static int vq_setup(struct virtqueue *vq) {
    mmio_write(vq->mmio, 0x030, vq->qidx);
    uint32_t qmax = mmio_read(vq->mmio, 0x034);
    if (qmax == 0 || qmax < VQ_SIZE) {
        kprintf("[virtio-blk] queue %d max=%u invalid\n", vq->qidx, qmax);
        return EINVAL;
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
    return 0;
}

static void vq_submit(struct virtqueue *vq, int desc_idx) {
    uint16_t slot = vq->avail->idx & (VQ_SIZE - 1);
    vq->avail->ring[slot] = desc_idx;
    fence();
    vq->avail->idx++;
    fence();
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
    kprintf("[virtio-blk] timeout waiting queue\n");
    return ETIMEDOUT;
}

static int vq_consume(struct virtqueue *vq) {
    if (vq->used->idx == vq->last_used)
        return EAGAIN;
    vq->last_used++;
    return 0;
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

int virtio_blk_ready(void) {
    return blk_ready;
}

int virtio_blk_init(void) {
    if (blk_ready)
        return 0;

    blk_mmio = find_blk_mmio();
    if (!blk_mmio)
        return ENODEV;

    mmio_write(blk_mmio, 0x070, 0);
    fence();
    mmio_write(blk_mmio, 0x070, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    fence();
    negotiate_features(blk_mmio);
    mmio_write(blk_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    fence();
    if (!(mmio_read(blk_mmio, 0x070) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio-blk] feature negotiation failed\n");
        mmio_write(blk_mmio, 0x070, 0);
        return EIO;
    }

    vq_bind(&blkq, blk_mmio, 0, blk_desc, &blk_avail, &blk_used);
    if (vq_setup(&blkq) != 0)
        return EIO;

    mmio_write(blk_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);
    fence();

    blk_ready = 1;
    kprintf("[virtio-blk] ready mmio=%x\n", (unsigned)(unsigned long)blk_mmio);
    return 0;
}

static int blk_transfer(unsigned long sector, void *buf, int write) {
    if (!blk_ready)
        return ENODEV;

    blk_outhdr.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    blk_outhdr.reserved = 0;
    blk_outhdr.sector = sector;
    blk_status = 0xff;

    if (write) {
        for (int i = 0; i < VIRTIO_SECTOR_SIZE; i++)
            blk_data[i] = ((char *)buf)[i];
    }

    blkq.last_used = blkq.used->idx;

    blk_desc[0].addr = (unsigned long)&blk_outhdr;
    blk_desc[0].len = sizeof(blk_outhdr);
    blk_desc[0].flags = VRING_DESC_F_NEXT;
    blk_desc[0].next = 1;

    blk_desc[1].addr = (unsigned long)blk_data;
    blk_desc[1].len = VIRTIO_SECTOR_SIZE;
    blk_desc[1].flags = write ? VRING_DESC_F_NEXT : (VRING_DESC_F_NEXT | VRING_DESC_F_WRITE);
    blk_desc[1].next = 2;

    blk_desc[2].addr = (unsigned long)&blk_status;
    blk_desc[2].len = 1;
    blk_desc[2].flags = VRING_DESC_F_WRITE;
    blk_desc[2].next = 0;

    vq_submit(&blkq, 0);

    if (vq_wait_used(&blkq, 500000) != 0)
        return ETIMEDOUT;
    vq_consume(&blkq);

    if (blk_status != 0)
        return EIO;

    if (!write) {
        for (int i = 0; i < VIRTIO_SECTOR_SIZE; i++)
            ((char *)buf)[i] = blk_data[i];
    }
    return 0;
}

int virtio_blk_read_sector(unsigned long sector, void *buf) {
    return blk_transfer(sector, buf, 0);
}

int virtio_blk_write_sector(unsigned long sector, const void *buf) {
    return blk_transfer(sector, (void *)buf, 1);
}
