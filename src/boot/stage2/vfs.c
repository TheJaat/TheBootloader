#include <vfs.h>
#include <ISO9660FS.h>
#include <print.h>

vfs_operations_t vfs_ops;

void init_vfs() {
    boot_print("[VFS] Initializing VFS\n");
    vfs_ops.mount = iso9660_mount;
    vfs_ops.open = iso9660_open;
    vfs_ops.read = iso9660_read;
    vfs_ops.list = iso9660_list;

    if(vfs_ops.mount() == 0) {
        boot_print("[VFS] Successfully Mounted\n");
        // vfs_ops.list("/");
    } else {
        boot_print("[VFS] Failed to Mount\n");
    }
    vfs_node_t *file = vfs_ops.open("ABC.TXT");
    if (file)
    {
        char buffer[512]; // Buffer to store file data
        int bytes = iso9660_read(file, buffer, sizeof(buffer), 0);
        if (bytes > 0)
        {
            boot_print("Read successful!\n");
            buffer[bytes] = '\0'; // Null-terminate for text files
            boot_print(buffer);
        }
        else
        {
            boot_print("Read failed!\n");
        }
    }
}
