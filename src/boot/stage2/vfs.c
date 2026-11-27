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

/* Ramdisk, loaded straight after the kernel. Zero when the image has no
 * ramdisk - that is not an error, the kernel simply boots without one. */
int g_ramdiskSize = 0;
unsigned int g_ramdiskAddress = 0;

/* Set by load_elf (elf.c), which runs before load_ramdisk. */
extern unsigned int g_kernelIs64;
extern unsigned int g_kernelEnd;
int usable_memory(unsigned long long base, unsigned long long end);

void init_vfs() {
    boot_print("[VFS] Initializing VFS\n");
    vfs_ops.mount = iso9660_mount;
    vfs_ops.open = iso9660_open;
    vfs_ops.read = iso9660_read;
    vfs_ops.list = iso9660_list;

    if(vfs_ops.mount() == 0) {
        boot_print("[VFS] Successfully Mounted\n");
    } else {
        boot_print("[VFS] Failed to Mount\n");
    }
    /* Here, until K23, every boot also opened SAAMPLE/RANDOM.TXT - a test
     * of the early reader, misspelled, never on the disc - and listed the
     * KERNEL directory on the screen. Under KVM every byte written to the
     * VGA text screen, and every scroll of it, is a trip out of the virtual
     * machine: the screen's chatter was most of the loader's second
     * (K23, journal entry NNN). */
}

/* The kernel's file, open, and its first 64 bytes - an ELF header, of
 * either class (K130). elf.c reads the rest through kernel_read. */
static vfs_node_t *g_kernelFile;
extern unsigned char g_kernelHead[64];

/* `size` bytes of the kernel's file from `offset`, to `dest`: what elf.c
 * reads a 64-bit kernel's program headers and segments with. The number
 * of bytes read, or -1. */
int kernel_read(void *dest, unsigned int size, unsigned int offset)
{
    return g_kernelFile ? vfs_ops.read(g_kernelFile, dest, size, offset) : -1;
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
    g_kernelFile = file;

    /* Its header first: a 64-bit kernel is not staged - load_elf reads
     * each of its segments from the file to where it goes (K130: the
     * whole file at 3 MiB, its segments below that, was a wall at 3 MiB
     * that every lane was nearing). Only a 32-bit kernel is read whole,
     * as before. */
    if (file->size < sizeof(g_kernelHead)
        || vfs_ops.read(file, g_kernelHead, sizeof(g_kernelHead), 0) != (int)sizeof(g_kernelHead)) {
        boot_print("Kernel read failed!\n");
        return 0;
    }
    if (g_kernelHead[4] == 2) {     /* EI_CLASS: ELFCLASS64 */
        boot_print("Kernel found: 64-bit, its segments are read in place\n");
        return 1;
    }

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

/* load_ramdisk
 * Reads RAMDISK.MDR into MEMORY_LOCATION_RAMDISK. Absence is not fatal:
 * the kernel checks RamDiskSize and carries on without one. */
int load_ramdisk() {
    boot_print("[VFS] Loading ramdisk\n");

    vfs_node_t *file = vfs_ops.open("RAMDISK.MDR");
    if (file == 0) {
        boot_print("[VFS] No RAMDISK.MDR, continuing without one\n");
        g_ramdiskSize = 0;
        g_ramdiskAddress = 0;
        return 1;   /* not an error */
    }

    /* The 32-bit kernel reads its ramdisk at a fixed address it was
     * compiled with, in a window of RAMDISK_MAX_SIZE. A 64-bit kernel is
     * told the address: its ramdisk goes on the next page after the
     * kernel (K130 - it went where the kernel's file was staged, which a
     * 64-bit kernel no longer is), in memory the firmware calls usable. */
    unsigned int address;
    if (g_kernelIs64) {
        address = (g_kernelEnd + 0xFFF) & ~0xFFFu;
        if (!usable_memory(address, (unsigned long long)address + file->size)) {
            boot_print("[VFS] No usable memory after the kernel for the ramdisk\n");
            g_ramdiskSize = 0;
            return 0;
        }
    } else {
        if (file->size > RAMDISK_MAX_SIZE) {
            boot_print("[VFS] Ramdisk is too large for its reserved window\n");
            g_ramdiskSize = 0;
            return 0;
        }
        address = RAMDISK_LOAD_ADDRESS;
    }
    unsigned char *load_address = (unsigned char *)address;
    int bytes = vfs_ops.read(file, load_address, file->size, 0);
    if (bytes != file->size) {
        boot_print("[VFS] Ramdisk read failed\n");
        g_ramdiskSize = 0;
        return 0;
    }

    g_ramdiskAddress = address;
    g_ramdiskSize = file->size;
    boot_print("[VFS] Ramdisk loaded\n");
    return 1;
}