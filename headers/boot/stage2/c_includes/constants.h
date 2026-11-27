#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

/* Where a 32-bit kernel's file is read to before its segments are copied
 * to their places. A 64-bit kernel is not staged at all since K130: its
 * segments are read from the file straight to where they go, so it may
 * be any size that fits in usable memory below 1 GiB. */
#define KERNEL_LOAD_START 0x300000

/* The ramdisk is loaded here and left in place - the kernel reads it
 * where it lies rather than copying it out. Must match
 * MEMORY_LOCATION_RAMDISK in the kernel's arch_x32.h, and must not
 * collide with KERNEL_LOAD_START above (the ELF staging buffer) or with
 * MEMORY_LOCATION_BITMAP, which is also 0x300000. */
#define RAMDISK_LOAD_ADDRESS 0x200000
#define RAMDISK_MAX_SIZE     0x100000   /* 1 mB, up to 0x300000 */

#endif /* __CONSTANTS_H__ */