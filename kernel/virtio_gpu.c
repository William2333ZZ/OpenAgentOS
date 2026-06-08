#include "virtio_gpu.h"
#include "printf.h"
#include "mem.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define VIRTIO_MAGIC   0x74726976
#define VIRTIO_DEV_GPU 16

#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define GPU_VQ_SIZE 8

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO        0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF          0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT             0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH          0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_RESP_OK_NODATA         0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO   0x1101

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[GPU_VQ_SIZE];
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[GPU_VQ_SIZE];
};

struct virtqueue {
    volatile uint32_t *mmio;
    int qidx;
    uint16_t last_used;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
};

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
        struct {
            uint32_t enabled;
            uint32_t r;
            uint32_t x;
            uint32_t y;
            uint32_t width;
            uint32_t height;
        } pmodes[16];
    } info;
};

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
};

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
};

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
};

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t scanout_id;
    uint32_t resource_id;
    uint32_t r_x;
    uint32_t r_y;
    uint32_t r_width;
    uint32_t r_height;
};

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t r_x;
    uint32_t r_y;
    uint32_t r_width;
    uint32_t r_height;
    uint32_t offset;
};

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t r_x;
    uint32_t r_y;
    uint32_t r_width;
    uint32_t r_height;
};

static volatile uint32_t *gpu_mmio;
static struct virtqueue gpu_ctrlq;
static int gpu_ready;
static uint32_t gpu_width = GPU_FB_WIDTH;
static uint32_t gpu_height = GPU_FB_HEIGHT;
static uint32_t gpu_resource_id = 1;
static uint32_t *fb_pixels;

static const uint8_t font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7f,0x36,0x7f,0x36,0x36,0x00},
    {0x0c,0x3e,0x03,0x1e,0x30,0x1f,0x0c,0x00},
    {0x00,0x63,0x33,0x18,0x0c,0x66,0x63,0x00},
    {0x1c,0x36,0x1c,0x6e,0x3b,0x33,0x6e,0x00},
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0c,0x06,0x06,0x06,0x0c,0x18,0x00},
    {0x06,0x0c,0x18,0x18,0x18,0x0c,0x06,0x00},
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},
    {0x00,0x0c,0x0c,0x3f,0x0c,0x0c,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0c,0x06,0x00},
    {0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x00},
    {0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00},
    {0x3e,0x63,0x73,0x7b,0x6f,0x67,0x3e,0x00},
    {0x0c,0x0e,0x0c,0x0c,0x0c,0x0c,0x3f,0x00},
    {0x1e,0x33,0x30,0x1c,0x06,0x33,0x3f,0x00},
    {0x1e,0x33,0x30,0x1c,0x30,0x33,0x1e,0x00},
    {0x38,0x3c,0x36,0x33,0x7f,0x30,0x78,0x00},
    {0x3f,0x03,0x1f,0x30,0x30,0x33,0x1e,0x00},
    {0x1c,0x06,0x03,0x1f,0x33,0x33,0x1e,0x00},
    {0x3f,0x33,0x30,0x18,0x0c,0x0c,0x0c,0x00},
    {0x1e,0x33,0x33,0x1e,0x33,0x33,0x1e,0x00},
    {0x1e,0x33,0x33,0x3e,0x30,0x18,0x0e,0x00},
    {0x00,0x0c,0x0c,0x00,0x00,0x0c,0x0c,0x00},
    {0x00,0x0c,0x0c,0x00,0x00,0x0c,0x06,0x00},
    {0x18,0x0c,0x06,0x03,0x06,0x0c,0x18,0x00},
    {0x00,0x00,0x3f,0x00,0x00,0x3f,0x00,0x00},
    {0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00},
    {0x1e,0x33,0x30,0x18,0x0c,0x0c,0x0c,0x00},
    {0x3e,0x63,0x7b,0x7b,0x7b,0x03,0x1e,0x00},
    {0x0c,0x1e,0x33,0x33,0x3f,0x33,0x33,0x00},
    {0x3f,0x66,0x66,0x3e,0x66,0x66,0x3f,0x00},
    {0x3c,0x66,0x03,0x03,0x03,0x66,0x3c,0x00},
    {0x1f,0x36,0x66,0x66,0x66,0x36,0x1f,0x00},
    {0x7f,0x46,0x16,0x1e,0x16,0x46,0x7f,0x00},
    {0x7f,0x46,0x16,0x1e,0x16,0x06,0x0f,0x00},
    {0x3c,0x66,0x03,0x03,0x73,0x66,0x7c,0x00},
    {0x33,0x33,0x33,0x3f,0x33,0x33,0x33,0x00},
    {0x1e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1e,0x00},
    {0x67,0x66,0x36,0x1e,0x36,0x66,0x67,0x00},
    {0x0f,0x06,0x06,0x06,0x46,0x66,0x7f,0x00},
    {0x63,0x77,0x7f,0x7f,0x6b,0x63,0x63,0x00},
    {0x63,0x67,0x6f,0x7b,0x73,0x63,0x63,0x00},
    {0x1c,0x36,0x63,0x63,0x63,0x36,0x1c,0x00},
    {0x3f,0x66,0x66,0x3e,0x06,0x06,0x0f,0x00},
    {0x3e,0x63,0x63,0x63,0x6b,0x3e,0x78,0x00},
    {0x3f,0x66,0x66,0x3e,0x36,0x66,0x67,0x00},
    {0x1e,0x33,0x07,0x0e,0x38,0x33,0x1e,0x00},
    {0x3f,0x2d,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3f,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1e,0x0c,0x00},
    {0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1c,0x1c,0x36,0x63,0x00},
    {0x33,0x33,0x33,0x1e,0x0c,0x0c,0x1e,0x00},
    {0x7f,0x63,0x31,0x18,0x4c,0x66,0x7f,0x00},
    {0x1e,0x06,0x06,0x06,0x06,0x06,0x1e,0x00},
    {0x03,0x06,0x0c,0x18,0x30,0x60,0x40,0x00},
    {0x1e,0x18,0x18,0x18,0x18,0x18,0x1e,0x00},
    {0x08,0x1c,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x0c,0x0c,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1e,0x30,0x3e,0x33,0x6e,0x00},
    {0x07,0x06,0x06,0x3e,0x66,0x66,0x3b,0x00},
    {0x00,0x00,0x1e,0x33,0x03,0x33,0x1e,0x00},
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6e,0x00},
    {0x00,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00},
    {0x1c,0x36,0x06,0x0f,0x06,0x06,0x0f,0x00},
    {0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x1f},
    {0x07,0x06,0x36,0x6e,0x66,0x66,0x67,0x00},
    {0x0c,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1e},
    {0x07,0x06,0x66,0x36,0x1e,0x36,0x67,0x00},
    {0x0e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x00,0x00,0x33,0x7f,0x7f,0x6b,0x63,0x00},
    {0x00,0x00,0x1f,0x33,0x33,0x33,0x33,0x00},
    {0x00,0x00,0x1e,0x33,0x33,0x33,0x1e,0x00},
    {0x00,0x00,0x3b,0x66,0x66,0x3e,0x06,0x0f},
    {0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x78},
    {0x00,0x00,0x3b,0x06,0x0e,0x06,0x1f,0x00},
    {0x00,0x00,0x3e,0x03,0x1e,0x30,0x1f,0x00},
    {0x00,0x00,0x0e,0x18,0x3e,0x18,0x0e,0x00},
    {0x00,0x00,0x63,0x63,0x63,0x63,0x6e,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1e,0x0c,0x00},
    {0x00,0x00,0x63,0x63,0x6b,0x7f,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1c,0x36,0x63,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x3e,0x30,0x1f},
    {0x00,0x00,0x3f,0x19,0x0c,0x26,0x3f,0x00},
    {0x1c,0x36,0x36,0x1c,0x36,0x36,0x1c,0x00},
    {0x0c,0x0c,0x3f,0x0c,0x0c,0x2c,0x18,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x06},
    {0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x00},
    {0x00,0x38,0x0c,0x06,0x03,0x06,0x0c,0x00},
};

static struct vring_desc ctrl_desc[GPU_VQ_SIZE] __attribute__((aligned(16)));
static struct vring_avail ctrl_avail __attribute__((aligned(4)));
static struct vring_used ctrl_used __attribute__((aligned(4)));

static inline uint32_t mmio_read(volatile uint32_t *base, int off) {
    return base[off / 4];
}

static inline void mmio_write(volatile uint32_t *base, int off, uint32_t val) {
    base[off / 4] = val;
}

static inline void fence(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
}

static volatile uint32_t *find_gpu_mmio(void) {
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
        if (devid != VIRTIO_DEV_GPU)
            continue;
        kprintf("[virtio-gpu] slot %d mmio=%lx\n", i, (unsigned long)base);
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
        if (qmax == 0 || qmax < GPU_VQ_SIZE) {
            kprintf("[virtio-gpu] queue %d max=%u invalid\n", vq->qidx, qmax);
            return;
        }
    }
    mmio_write(vq->mmio, 0x038, GPU_VQ_SIZE);
    mmio_write(vq->mmio, 0x080, (uint32_t)(unsigned long)vq->desc);
    mmio_write(vq->mmio, 0x084, (uint32_t)((unsigned long)vq->desc >> 32));
    mmio_write(vq->mmio, 0x090, (uint32_t)(unsigned long)vq->avail);
    mmio_write(vq->mmio, 0x094, (uint32_t)((unsigned long)vq->avail >> 32));
    mmio_write(vq->mmio, 0x0a0, (uint32_t)(unsigned long)vq->used);
    mmio_write(vq->mmio, 0x0a4, (uint32_t)((unsigned long)vq->used >> 32));
    fence();
    mmio_write(vq->mmio, 0x044, 1);
}

static void vq_submit_chain(struct virtqueue *vq, int head, int ndesc) {
    (void)ndesc;
    uint16_t slot = vq->avail->idx & (GPU_VQ_SIZE - 1);
    vq->avail->ring[slot] = (uint16_t)head;
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
    return ETIMEDOUT;
}

static int gpu_cmd(void *req, int req_len, void *resp, int resp_len) {
    int head = 0;
    int rc;

    gpu_ctrlq.desc[0].addr = (unsigned long)req;
    gpu_ctrlq.desc[0].len = (uint32_t)req_len;
    gpu_ctrlq.desc[0].flags = VRING_DESC_F_NEXT;
    gpu_ctrlq.desc[0].next = 1;

    gpu_ctrlq.desc[1].addr = (unsigned long)resp;
    gpu_ctrlq.desc[1].len = (uint32_t)resp_len;
    gpu_ctrlq.desc[1].flags = VRING_DESC_F_WRITE;
    gpu_ctrlq.desc[1].next = 0;

    vq_submit_chain(&gpu_ctrlq, head, 2);
    rc = vq_wait_used(&gpu_ctrlq, 500000);
    if (rc != 0)
        return rc;
    gpu_ctrlq.last_used++;
    return 0;
}

static int gpu_setup_framebuffer(void) {
    struct virtio_gpu_resource_create_2d create;
    struct {
        struct virtio_gpu_resource_attach_backing attach;
        struct virtio_gpu_mem_entry entry;
    } attach_cmd;
    struct virtio_gpu_set_scanout scanout;
    struct virtio_gpu_ctrl_hdr resp;
    int rc;

    create.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create.hdr.flags = 0;
    create.hdr.fence_id = 0;
    create.hdr.ctx_id = 0;
    create.hdr.padding = 0;
    create.resource_id = gpu_resource_id;
    create.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    create.width = gpu_width;
    create.height = gpu_height;

    rc = gpu_cmd(&create, (int)sizeof(create), &resp, (int)sizeof(resp));
    if (rc != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[virtio-gpu] create 2d failed rc=%d type=0x%x\n", rc, resp.type);
        return EIO;
    }

    attach_cmd.attach.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_cmd.attach.hdr.flags = 0;
    attach_cmd.attach.hdr.fence_id = 0;
    attach_cmd.attach.hdr.ctx_id = 0;
    attach_cmd.attach.hdr.padding = 0;
    attach_cmd.attach.resource_id = gpu_resource_id;
    attach_cmd.attach.nr_entries = 1;
    attach_cmd.entry.addr = (uint64_t)(unsigned long)fb_pixels;
    attach_cmd.entry.length = gpu_width * gpu_height * 4;
    attach_cmd.entry.padding = 0;

    rc = gpu_cmd(&attach_cmd, (int)sizeof(attach_cmd), &resp, (int)sizeof(resp));
    if (rc != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[virtio-gpu] attach backing failed rc=%d type=0x%x\n", rc, resp.type);
        return EIO;
    }

    scanout.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout.hdr.flags = 0;
    scanout.hdr.fence_id = 0;
    scanout.hdr.ctx_id = 0;
    scanout.hdr.padding = 0;
    scanout.scanout_id = 0;
    scanout.resource_id = gpu_resource_id;
    scanout.r_x = 0;
    scanout.r_y = 0;
    scanout.r_width = gpu_width;
    scanout.r_height = gpu_height;

    rc = gpu_cmd(&scanout, (int)sizeof(scanout), &resp, (int)sizeof(resp));
    if (rc != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[virtio-gpu] set scanout warn rc=%d type=0x%x (continuing)\n", rc,
                resp.type);
    }
    return 0;
}

void virtio_gpu_init(void) {
    struct virtio_gpu_resp_display_info dinfo;
    struct virtio_gpu_ctrl_hdr req;
    int rc;
    int i;

    if (gpu_ready)
        return;

    gpu_mmio = find_gpu_mmio();
    if (!gpu_mmio) {
        kprintf("[virtio-gpu] no virtio-gpu device\n");
        return;
    }

    mmio_write(gpu_mmio, 0x070, 0);
    fence();
    mmio_write(gpu_mmio, 0x070, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    fence();
    mmio_write(gpu_mmio, 0x014, 0);
    mmio_write(gpu_mmio, 0x014, 1);
    (void)mmio_read(gpu_mmio, 0x010);
    (void)mmio_read(gpu_mmio, 0x010);
    mmio_write(gpu_mmio, 0x024, 0);
    mmio_write(gpu_mmio, 0x020, 0);
    mmio_write(gpu_mmio, 0x024, 1);
    mmio_write(gpu_mmio, 0x020, 0);
    mmio_write(gpu_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    fence();
    if (!(mmio_read(gpu_mmio, 0x070) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[virtio-gpu] feature negotiation failed\n");
        mmio_write(gpu_mmio, 0x070, 0);
        return;
    }

    vq_bind(&gpu_ctrlq, gpu_mmio, 0, ctrl_desc, &ctrl_avail, &ctrl_used);
    vq_setup(&gpu_ctrlq);

    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    req.flags = 0;
    req.fence_id = 0;
    req.ctx_id = 0;
    req.padding = 0;
    rc = gpu_cmd(&req, (int)sizeof(req), &dinfo, (int)sizeof(dinfo));
    if (rc == 0 && dinfo.hdr.type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO &&
        dinfo.info.pmodes[0].enabled) {
        if (dinfo.info.pmodes[0].width > 0 && dinfo.info.pmodes[0].height > 0) {
            gpu_width = dinfo.info.pmodes[0].width;
            gpu_height = dinfo.info.pmodes[0].height;
        }
    }
    if (gpu_width > GPU_FB_WIDTH)
        gpu_width = GPU_FB_WIDTH;
    if (gpu_height > GPU_FB_HEIGHT)
        gpu_height = GPU_FB_HEIGHT;

    fb_pixels = kalloc((unsigned long)gpu_width * gpu_height * 4);
    if (!fb_pixels) {
        kprintf("[virtio-gpu] framebuffer alloc failed\n");
        mmio_write(gpu_mmio, 0x070, 0);
        return;
    }

    for (i = 0; i < (int)(gpu_width * gpu_height); i++)
        fb_pixels[i] = 0xff102030;

    if (gpu_setup_framebuffer() != 0) {
        mmio_write(gpu_mmio, 0x070, 0);
        return;
    }

    mmio_write(gpu_mmio, 0x070,
               VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);
    fence();
    gpu_ready = 1;
    kprintf("[virtio-gpu] driver ready %ux%u\n", gpu_width, gpu_height);
}

int virtio_gpu_ready(void) {
    return gpu_ready;
}

void virtio_gpu_clear(uint32_t color) {
    int i;
    if (!gpu_ready || !fb_pixels)
        return;
    for (i = 0; i < (int)(gpu_width * gpu_height); i++)
        fb_pixels[i] = color;
}

static void gpu_put_pixel(int x, int y, uint32_t color) {
    if (!fb_pixels)
        return;
    if (x < 0 || y < 0 || (uint32_t)x >= gpu_width || (uint32_t)y >= gpu_height)
        return;
    fb_pixels[y * (int)gpu_width + x] = color;
}

void virtio_gpu_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg) {
    int cx = x;
    int cy = y;
    int i;
    int c;
    int row;
    int col;

    if (!gpu_ready || !text)
        return;

    for (i = 0; text[i]; i++) {
        c = (unsigned char)text[i];
        if (c == '\n') {
            cx = x;
            cy += 10;
            continue;
        }
        if (c < 32 || c > 127)
            c = '?';
        c -= 32;
        for (row = 0; row < 8; row++) {
            uint8_t bits = font8x8[c][row];
            for (col = 0; col < 8; col++) {
                gpu_put_pixel(cx + col, cy + row, (bits & (1 << col)) ? fg : bg);
            }
        }
        cx += 8;
    }
}

int virtio_gpu_flush(void) {
    struct virtio_gpu_transfer_to_host_2d xfer;
    struct virtio_gpu_resource_flush flush;
    struct virtio_gpu_ctrl_hdr resp;
    int rc;

    if (!gpu_ready)
        return ENODEV;

    xfer.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    xfer.hdr.flags = 0;
    xfer.hdr.fence_id = 0;
    xfer.hdr.ctx_id = 0;
    xfer.hdr.padding = 0;
    xfer.resource_id = gpu_resource_id;
    xfer.r_x = 0;
    xfer.r_y = 0;
    xfer.r_width = gpu_width;
    xfer.r_height = gpu_height;
    xfer.offset = 0;

    rc = gpu_cmd(&xfer, (int)sizeof(xfer), &resp, (int)sizeof(resp));
    if (rc != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[virtio-gpu] transfer warn rc=%d type=0x%x\n", rc, resp.type);
        return 0;
    }

    flush.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush.hdr.flags = 0;
    flush.hdr.fence_id = 0;
    flush.hdr.ctx_id = 0;
    flush.hdr.padding = 0;
    flush.resource_id = gpu_resource_id;
    flush.r_x = 0;
    flush.r_y = 0;
    flush.r_width = gpu_width;
    flush.r_height = gpu_height;

    rc = gpu_cmd(&flush, (int)sizeof(flush), &resp, (int)sizeof(resp));
    if (rc != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[virtio-gpu] flush warn rc=%d type=0x%x\n", rc, resp.type);
        return 0;
    }
    return 0;
}
