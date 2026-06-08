#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#define VIRTIO_SECTOR_SIZE 512

int virtio_blk_init(void);
int virtio_blk_ready(void);
int virtio_blk_read_sector(unsigned long sector, void *buf);
int virtio_blk_write_sector(unsigned long sector, const void *buf);

#endif
