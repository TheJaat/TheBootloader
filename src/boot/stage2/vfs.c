#include <vfs.h>
#include <ISO9660FS.h>
#include <print.h>
#include <constants.h>
#include <stdint.h>

vfs_operations_t vfs_ops;
/*
* This global variable stores the kernel size,
* It is used in stage2.asm as an external.
* It's value then stored in OsBootDescriptor structure.
*/
int g_kernelSize;

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
    vfs_node_t *file = vfs_ops.open("SAAMPLE/RANDOM.TXT");
    if (file)
    {
        char buffer[512]; // Buffer to store file data
        int bytes = vfs_ops.read(file, buffer, sizeof(buffer), 0);
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

    // List all the directory and files of KERNEL directory.
    vfs_ops.list("KERNEL");
}

int load_kernel(const char* kernel_name)
{
    vfs_node_t *file = vfs_ops.open(kernel_name);
    if (!file)
    {
        boot_print("Failed to open KERNEL/KERNEL.ELF\n");
        return 0;
    }
    // Store the kernel size in global variable which would be access in stage2.asm
	g_kernelSize = file->size;

    boot_print("Loading KERNEL.ELF at 0x300000\n");

    unsigned char *load_address = (unsigned char *)KERNEL_LOAD_START;//0x300000;
    int bytes = vfs_ops.read(file, load_address, file->size, 0);

    if (bytes != file->size)
    {
        boot_print("Kernel read failed!\n");
        return 0;     // False
    }
    else
    {
        boot_print("Kernel loaded successfully!\n");
        return 1;     // True
    }

    // We can jump to its directly if it is binary kernel.
    // uint32_t kernel_address = KERNEL_LOAD_START;
	// void (*kernel_entry)(void) = (void (*)(void))0x300000;
	// kernel_entry();
}
