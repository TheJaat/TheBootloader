#ifndef _VFS_H_
#define _VFS_H_

typedef struct vfs_node {
    char name[128];
    unsigned int size;
    unsigned int sector;
    unsigned char is_directory;
    struct vfs_node* next;
} vfs_node_t;

typedef struct {
    int (*mount)(); // mount the file system
   vfs_node_t* (*open)(const char* path);
   int (*read)(vfs_node_t* node, void* buffer, unsigned int size, unsigned int offset);
   void (*list)(const char* path);
} vfs_operations_t;

void init_vfs();

void load_kernel()

#endif //  _VFS_H_
