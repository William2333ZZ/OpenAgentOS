#ifndef RAMFS_H
#define RAMFS_H

#define RAMFS_PATH_MAX   64
#define RAMFS_FILE_SIZE  8192
#define RAMFS_MAX_FILES  64

void ramfs_init(void);
int ramfs_read(const char *path, unsigned long offset, char *buf, int len);
int ramfs_write(const char *path, unsigned long offset, const char *data, int len);

int ramfs_put(const char *path, const char *data, int size);
int ramfs_path_exists(const char *path);
int ramfs_count_agent_files(int agent_id);
void ramfs_clear_user(void);
int ramfs_user_count(void);
int ramfs_user_get(int idx, char *path, char *data, int cap);

#endif
