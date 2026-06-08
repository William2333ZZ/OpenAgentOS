#include "persist.h"
#include "ramfs.h"
#include "virtio_blk.h"
#include "printf.h"
#include "mem.h"
#include "../include/agentos.h"

#define PERSIST_MAGIC     0x31495641u /* on-disk AOS tag (v1/v2) */
#define PERSIST_VERSION_AOS1 1
#define PERSIST_VERSION_AOS2 2

struct persist_super {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t checksum;
};

struct persist_meta {
    char path[RAMFS_PATH_MAX];
    uint32_t size;
    uint32_t crc32;
};

static int blk_ok;
static char persist_sector_buf[VIRTIO_SECTOR_SIZE];
static char persist_io_buf[RAMFS_FILE_SIZE];

static void *memcpy_local(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

static uint32_t persist_crc32(const unsigned char *data, unsigned long len) {
    uint32_t crc = 0xffffffffu;
    unsigned long i;
    int bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xedb88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

static int persist_read_file_data(unsigned long data_sector, char *data, int size) {
    int copied = 0;

    while (copied < size) {
        if (virtio_blk_read_sector(data_sector++, persist_sector_buf) != 0)
            return EIO;
        {
            int chunk = size - copied;
            if (chunk > VIRTIO_SECTOR_SIZE)
                chunk = VIRTIO_SECTOR_SIZE;
            memcpy_local(data + copied, persist_sector_buf, (unsigned long)chunk);
            copied += chunk;
        }
    }
    return 0;
}

static int persist_write_file_data(unsigned long data_sector, const char *data, int size) {
    int copied = 0;

    while (copied < size) {
        int chunk = size - copied;
        int j;

        for (j = 0; j < VIRTIO_SECTOR_SIZE; j++)
            persist_sector_buf[j] = 0;
        if (chunk > VIRTIO_SECTOR_SIZE)
            chunk = VIRTIO_SECTOR_SIZE;
        memcpy_local(persist_sector_buf, data + copied, (unsigned long)chunk);
        if (virtio_blk_write_sector(data_sector++, persist_sector_buf) != 0)
            return EIO;
        copied += chunk;
    }
    return 0;
}

static unsigned long persist_file_sectors(int size) {
    return (unsigned long)((size + VIRTIO_SECTOR_SIZE - 1) / VIRTIO_SECTOR_SIZE);
}

static int persist_read_super(struct persist_super *sup) {
    if (virtio_blk_read_sector(0, persist_sector_buf) != 0)
        return EIO;
    memcpy_local(sup, persist_sector_buf, sizeof(*sup));
    return 0;
}

static int persist_write_super(const struct persist_super *sup) {
    int j;

    for (j = 0; j < VIRTIO_SECTOR_SIZE; j++)
        persist_sector_buf[j] = 0;
    memcpy_local(persist_sector_buf, sup, sizeof(*sup));
    return virtio_blk_write_sector(0, persist_sector_buf);
}

static int persist_read_meta(unsigned long sector, struct persist_meta *meta) {
    if (virtio_blk_read_sector(sector, persist_sector_buf) != 0)
        return EIO;
    memcpy_local(meta, persist_sector_buf, sizeof(*meta));
    return 0;
}

static int persist_write_meta(unsigned long sector, const struct persist_meta *meta) {
    int j;

    for (j = 0; j < VIRTIO_SECTOR_SIZE; j++)
        persist_sector_buf[j] = 0;
    memcpy_local(persist_sector_buf, meta, sizeof(*meta));
    return virtio_blk_write_sector(sector, persist_sector_buf);
}

static int persist_load_v1(unsigned long file_count) {
    unsigned long sector = 1;
    int i;

    for (i = 0; i < (int)file_count; i++) {
        struct persist_meta meta;
        char *data = persist_io_buf;
        unsigned long data_sector;

        if (persist_read_meta(sector++, &meta) != 0)
            return EIO;
        if (meta.size <= 0 || meta.size > RAMFS_FILE_SIZE)
            return EIO;

        data_sector = sector;
        sector += persist_file_sectors((int)meta.size);

        if (persist_read_file_data(data_sector, data, (int)meta.size) != 0)
            return EIO;
        data[meta.size] = '\0';
        if (ramfs_put(meta.path, data, (int)meta.size) != 0)
            return EIO;
        kprintf("[persist] restored %s (%u bytes)\n", meta.path, meta.size);
    }
    return 0;
}

static int persist_load_v2(unsigned long file_count, uint32_t super_crc) {
    unsigned long sector = 1;
    uint32_t rolling = 0;
    int i;

    for (i = 0; i < (int)file_count; i++) {
        struct persist_meta meta;
        char *data = persist_io_buf;
        unsigned long data_sector;
        uint32_t crc;

        if (persist_read_meta(sector++, &meta) != 0)
            return EIO;
        if (meta.size <= 0 || meta.size > RAMFS_FILE_SIZE)
            return EIO;

        rolling ^= meta.crc32;
        rolling ^= (uint32_t)meta.size;

        data_sector = sector;
        sector += persist_file_sectors((int)meta.size);

        if (persist_read_file_data(data_sector, data, (int)meta.size) != 0)
            return EIO;
        data[meta.size] = '\0';

        crc = persist_crc32((const unsigned char *)data, (unsigned long)meta.size);
        if (crc != meta.crc32) {
            kprintf("[persist] crc mismatch path=%s\n", meta.path);
            return EIO;
        }

        if (ramfs_put(meta.path, data, (int)meta.size) != 0)
            return EIO;
        kprintf("[persist] restored %s (%u bytes, crc ok)\n", meta.path, meta.size);
    }

    if (rolling != super_crc) {
        kprintf("[persist] super checksum mismatch\n");
        return EIO;
    }
    return 0;
}

void persist_init(void) {
    blk_ok = (virtio_blk_init() == 0);
    if (blk_ok)
        kprintf("[persist] virtio block ready\n");
    else
        kprintf("[persist] no block device (ramfs only)\n");
}

int persist_ready(void) {
    return blk_ok;
}

int persist_migrate(void) {
    if (!blk_ok)
        return ENODEV;
    kprintf("[persist] migrate AOS1 -> AOS2\n");
    return persist_sync();
}

int persist_load(void) {
    struct persist_super sup;
    int rc;
    int migrated = 0;

    if (!blk_ok)
        return 0;

    if (persist_read_super(&sup) != 0)
        return EIO;
    if (sup.magic != PERSIST_MAGIC)
        return 0;

    if (sup.version == PERSIST_VERSION_AOS1) {
        kprintf("[persist] loading %u files from AOS1 block\n", sup.file_count);
        ramfs_clear_user();
        rc = persist_load_v1(sup.file_count);
        if (rc != 0)
            return rc;
        migrated = 1;
    } else if (sup.version == PERSIST_VERSION_AOS2) {
        kprintf("[persist] loading %u files from AOS2 block\n", sup.file_count);
        ramfs_clear_user();
        rc = persist_load_v2(sup.file_count, sup.checksum);
        if (rc != 0)
            return rc;
    } else {
        return EIO;
    }

    if (migrated)
        return persist_migrate();
    return 0;
}

int persist_sync(void) {
    struct persist_super sup;
    int count = ramfs_user_count();
    unsigned long sector = 1;
    uint32_t rolling = 0;
    int idx;

    if (!blk_ok)
        return ENODEV;

    sup.magic = PERSIST_MAGIC;
    sup.version = PERSIST_VERSION_AOS2;
    sup.file_count = (uint32_t)count;
    sup.checksum = 0;

    for (idx = 0; idx < count; idx++) {
        struct persist_meta meta;
        char *data = persist_io_buf;
        int size;
        unsigned long data_sector;

        size = ramfs_user_get(idx, meta.path, data, RAMFS_FILE_SIZE);
        if (size < 0)
            return EIO;
        meta.size = (uint32_t)size;
        meta.crc32 = persist_crc32((const unsigned char *)data, (unsigned long)size);
        rolling ^= meta.crc32;
        rolling ^= (uint32_t)meta.size;

        if (persist_write_meta(sector++, &meta) != 0)
            return EIO;

        data_sector = sector;
        sector += persist_file_sectors(size);

        if (persist_write_file_data(data_sector, data, size) != 0)
            return EIO;
        kprintf("[persist] saved %s (%d bytes, aos2)\n", meta.path, size);
    }

    sup.checksum = rolling;
    if (persist_write_super(&sup) != 0)
        return EIO;

    kprintf("[persist] sync complete AOS2 (%d files, %lu sectors)\n", count, sector);
    return 0;
}
