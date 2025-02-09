#ifndef _ISO9660_FS_H
#define _ISO9660_FS_H

#include <vfs.h>

#define ISO9660_SECTOR_SIZE 2048
#define ISO9660_PVD_SECTOR 16

typedef struct {
    unsigned int root_dir_sector;
    unsigned int root_dir_size
} iso9660_fs_t;

int iso9660_mount();
vfs_node_t* iso9660_open(const char* path);
int iso9660_read(vfs_node_t* node, void* buffer, unsigned int size, unsigned int offset);
void iso9660_list(const char* path);

#endif // _ISO9660_FS_H
