#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

/* Where the kernel's file is read to before its segments are copied
 * to their places. Once that is done the buffer is free, and a 64-bit
 * kernel's ramdisk is loaded into it: such a kernel may occupy 1 MiB up
 * to here, across RAMDISK_LOAD_ADDRESS below. */
#define KERNEL_LOAD_START 0x300000

/* The ramdisk is loaded here and left in place - the kernel reads it
 * where it lies rather than copying it out. Must match
 * MEMORY_LOCATION_RAMDISK in the kernel's arch_x32.h, and must not
 * collide with KERNEL_LOAD_START above (the ELF staging buffer) or with
 * MEMORY_LOCATION_BITMAP, which is also 0x300000. */
#define RAMDISK_LOAD_ADDRESS 0x200000
#define RAMDISK_MAX_SIZE     0x100000   /* 1 mB, up to 0x300000 */

#endif /* __CONSTANTS_H__ */