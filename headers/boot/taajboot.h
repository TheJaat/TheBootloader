#ifndef __TAAJBOOT_H__
#define __TAAJBOOT_H__

/* The TaajBoot protocol: what every TheTaaJ bootloader hands a 64-bit
 * kernel.
 *
 * The kernel must not know or care how it was booted. BIOS, UEFI and,
 * later, U-Boot loaders each do whatever their firmware demands and
 * then fill in this one structure. A new way to boot is then a new
 * loader, never a change to the kernel.
 *
 * Rules that keep it stable across loaders, compilers and versions:
 *
 *   - fixed-width fields only, with explicit padding, so C (32-bit and
 *     64-bit) and Rust agree on every offset. The kernel's Rust mirror
 *     (kernel64/src/bootinfo.rs) asserts the offsets at compile time.
 *   - every address is a PHYSICAL address in a 64-bit field, even when
 *     the loader is 32-bit code
 *   - fields are only ever appended. `Size` lets a newer kernel tell
 *     which fields an older loader filled in.
 *
 * Machine state on entry to the kernel (x86_64):
 *
 *   - 64-bit long mode, interrupts disabled, no IDT the kernel may rely
 *     on
 *   - RDI = physical address of the TaajBootInfo
 *   - paging on, with two mappings, both using 2 MiB pages:
 *       identity      virtual 0 .. 4 GiB  -> physical 0 .. 4 GiB
 *       kernel window virtual 0xFFFFFFFF80000000 + x -> physical x,
 *                     for x in 0 .. 1 GiB
 *     The identity map exists so the kernel can read this structure and
 *     the framebuffer before it has page tables of its own. It should
 *     build its own and stop relying on these early.
 *   - everything below 1 MiB belongs to the loader: this structure, the
 *     memory map, the GDT and the initial page tables live there. The
 *     kernel must not hand that memory out until it has copied what it
 *     needs and switched to its own page tables.
 */

#include <stdint.h>

/* "TAAJBOOT" as little-endian bytes. */
#define TAAJ_BOOT_MAGIC         0x544F4F424A414154ULL
#define TAAJ_BOOT_VERSION       1

#define TAAJ_BOOT_BIOS          1
#define TAAJ_BOOT_UEFI          2

/* Memory region types. 1-5 match the BIOS E820 types on purpose, so the
 * BIOS loader passes them through unchanged; UEFI types are translated
 * into these. */
#define TAAJ_MEMORY_USABLE      1
#define TAAJ_MEMORY_RESERVED    2
#define TAAJ_MEMORY_ACPI_RECLAIMABLE 3
#define TAAJ_MEMORY_ACPI_NVS    4
#define TAAJ_MEMORY_BAD         5

typedef struct _TaajMemoryRegion {
    uint64_t Base;
    uint64_t Length;
    uint32_t Type;
    uint32_t Reserved;
} TaajMemoryRegion_t;                           /* 24 bytes */

typedef struct _TaajFramebuffer {
    uint64_t Address;           /* physical; 0 = no framebuffer */
    uint32_t Width;             /* pixels */
    uint32_t Height;
    uint32_t Pitch;             /* bytes per line - NOT Width * 4 */
    uint8_t  Bpp;
    uint8_t  RedSize,   RedShift;
    uint8_t  GreenSize, GreenShift;
    uint8_t  BlueSize,  BlueShift;
    uint8_t  Reserved;
    uint32_t Padding;
} TaajFramebuffer_t;                            /* 32 bytes */

typedef struct _TaajBootInfo {
    uint64_t Magic;             /*   0  TAAJ_BOOT_MAGIC                 */
    uint32_t Version;           /*   8  TAAJ_BOOT_VERSION                */
    uint32_t Size;              /*  12  sizeof(TaajBootInfo_t)           */

    uint64_t KernelPhysical;    /*  16  lowest physical byte loaded      */
    uint64_t KernelVirtual;     /*  24  where that byte is mapped        */
    uint64_t KernelSize;        /*  32  span of the image, .bss included */

    uint64_t RamdiskAddress;    /*  40  physical; 0 = none               */
    uint64_t RamdiskSize;       /*  48                                   */

    uint64_t MemoryMap;         /*  56  physical TaajMemoryRegion_t[]    */
    uint32_t MemoryMapCount;    /*  64                                   */
    uint32_t BootMethod;        /*  68  TAAJ_BOOT_BIOS / TAAJ_BOOT_UEFI   */

    uint64_t AcpiRsdp;          /*  72  physical; 0 = not found          */
    uint64_t DeviceTree;        /*  80  physical; 0 = none (x86)         */
    uint64_t CommandLine;       /*  88  physical C string; 0 = none      */

    TaajFramebuffer_t Framebuffer; /* 96                                 */

    uint64_t LoaderName;        /* 128  physical C string                */
} TaajBootInfo_t;                               /* 136 bytes */

/* The same promise the Rust side makes, checked here too: if a field
 * is ever added in the middle, both sides fail to compile. */
typedef char TaajBootInfoSizeCheck[(sizeof(TaajBootInfo_t) == 136) ? 1 : -1];
typedef char TaajFramebufferSizeCheck[(sizeof(TaajFramebuffer_t) == 32) ? 1 : -1];
typedef char TaajMemoryRegionSizeCheck[(sizeof(TaajMemoryRegion_t) == 24) ? 1 : -1];

#endif /* __TAAJBOOT_H__ */
