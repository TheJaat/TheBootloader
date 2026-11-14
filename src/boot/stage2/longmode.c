/* longmode.c - everything a 64-bit kernel needs before stage 2 jumps.
 *
 * Runs in 32-bit protected mode, after the VESA mode is set, and only
 * when load_elf() found an ELF64 kernel. It does three things:
 *
 *   1. checks the CPU can run 64-bit code at all
 *   2. fills in the TaajBootInfo (see taajboot.h) from what the earlier
 *      real-mode code collected: the E820 map, the VESA mode, and the
 *      kernel and ramdisk placement
 *   3. builds the page tables long mode requires, and returns the
 *      address of the top one for longmode.asm to load into CR3
 *
 * The mode switch itself is a handful of control-register writes that
 * C cannot express, so it lives in longmode.asm.
 */

#include <stdint.h>
#include <print.h>
#include <mem.h>
#include <boot/taajboot.h>

/* ---- what the rest of stage 2 left for us ------------------------ */

/* elf.c */
extern unsigned int       g_kernelAddress;  /* lowest physical byte */
extern unsigned int       g_kernelEnd;      /* one past the highest */
extern unsigned long long g_kernelVirtual;
/* vfs.c */
extern unsigned int       g_ramdiskAddress;
extern int                g_ramdiskSize;

/* The MultiBoot structure stage2.asm fills in real mode - only the
 * fields used here. Offsets are from datastructure.inc. */
#define MB_MEMMAP_LENGTH    (11 * 4)    /* number of E820 entries   */
#define MB_MEMMAP_ADDR      (12 * 4)    /* where they were stored   */
#define MB_VBE_MODE         (20 * 4)    /* 0 = no mode was chosen   */

/* One entry as LoadMemoryMap stores it: the 24 bytes E820 returns,
 * padded to 32 (memory.inc advances by 0x20). */
typedef struct {
    uint64_t Base;
    uint64_t Length;
    uint32_t Type;
    uint32_t Acpi;
    uint32_t Pad[2];
} E820Entry_t;

/* The VBE mode-info fields used here, at their offsets in the 256-byte
 * block INT 10h/4F01h fills in. */
#define VBE_PITCH           16      /* uint16 BytesPerScanline */
#define VBE_WIDTH           18      /* uint16 */
#define VBE_HEIGHT          20      /* uint16 */
#define VBE_BPP             25      /* uint8  */
#define VBE_RED_SIZE        31      /* then position, green, blue: uint8 each */
#define VBE_FRAMEBUFFER     40      /* uint32 physical LFB address */

/* ---- what this file hands to the kernel -------------------------- */

#define MEMORY_REGIONS_MAX  64

/* Plain globals: stage 2 runs at fixed low addresses with paging off,
 * so a global's address IS its physical address - exactly what the
 * protocol wants. longmode.asm reads g_bootInfo's address into RDI. */
TaajBootInfo_t            g_bootInfo;
static TaajMemoryRegion_t MemoryRegions[MEMORY_REGIONS_MAX];
static const char         LoaderName[] = "TheTaaJBoot";

/* ---- page tables ------------------------------------------------- */

/* The kernel window, as in elf.c and taajboot.h. */
#define KERNEL_WINDOW_PML4  511     /* 0xFFFFFFFF80000000 is in the last  */
#define KERNEL_WINDOW_PDPT  510     /* 512 GiB, second-to-last GiB of it */

#define PAGE_PRESENT        0x001ULL
#define PAGE_WRITABLE       0x002ULL
#define PAGE_HUGE           0x080ULL    /* in a PD entry: a 2 MiB page */
#define TWO_MIB             0x200000ULL

/* Every table is one 4 KiB page of 512 entries, and must be 4 KiB
 * aligned: the CPU finds the next level from an entry's top bits and
 * assumes the low 12 are flags.
 *
 *   Pml4                  the top level, loaded into CR3
 *   IdentityPdpt          first 512 GiB  -> 4 PDs below
 *   IdentityPd[4]         0 .. 4 GiB in 2 MiB pages
 *   WindowPdpt            last 512 GiB   -> 1 PD below
 *   WindowPd              the kernel window, 1 GiB in 2 MiB pages
 *
 * 2 MiB pages mean no fourth level (page tables proper) at all: eight
 * pages of tables map 5 GiB. */
static uint64_t Pml4[512]          __attribute__((aligned(4096)));
static uint64_t IdentityPdpt[512]  __attribute__((aligned(4096)));
static uint64_t IdentityPd[4][512] __attribute__((aligned(4096)));
static uint64_t WindowPdpt[512]    __attribute__((aligned(4096)));
static uint64_t WindowPd[512]      __attribute__((aligned(4096)));

/* These live in .bss, which the flat binary does not contain - so their
 * memory holds whatever was there before. Zero them first, always. */
static void BuildPageTables(void)
{
    int i, j;

    memsetb(Pml4, 0, sizeof(Pml4));
    memsetb(IdentityPdpt, 0, sizeof(IdentityPdpt));
    memsetb(IdentityPd, 0, sizeof(IdentityPd));
    memsetb(WindowPdpt, 0, sizeof(WindowPdpt));
    memsetb(WindowPd, 0, sizeof(WindowPd));

    /* Identity: covers stage 2 itself (whose next instruction must still
     * be there once paging turns on), this structure, and a framebuffer
     * anywhere below 4 GiB. */
    Pml4[0] = (uint32_t)IdentityPdpt | PAGE_PRESENT | PAGE_WRITABLE;
    for (i = 0; i < 4; i++) {
        IdentityPdpt[i] = (uint32_t)IdentityPd[i] | PAGE_PRESENT | PAGE_WRITABLE;
        for (j = 0; j < 512; j++) {
            IdentityPd[i][j] = ((uint64_t)(i * 512 + j) * TWO_MIB)
                             | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
        }
    }

    /* The kernel window: where a higher-half kernel is linked. */
    Pml4[KERNEL_WINDOW_PML4] = (uint32_t)WindowPdpt | PAGE_PRESENT | PAGE_WRITABLE;
    WindowPdpt[KERNEL_WINDOW_PDPT] = (uint32_t)WindowPd | PAGE_PRESENT | PAGE_WRITABLE;
    for (j = 0; j < 512; j++) {
        WindowPd[j] = ((uint64_t)j * TWO_MIB) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
    }
}

/* ---- CPU check ----------------------------------------------------- */

static void Cpuid(uint32_t Leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(Leaf), "c"(0));
}

/* Long mode is advertised in extended leaf 0x80000001, EDX bit 29. The
 * extended range has to be asked about first: on a CPU without it, the
 * leaf returns garbage rather than an error. */
static int LongModeSupported(void)
{
    uint32_t a, b, c, d;

    Cpuid(0x80000000, &a, &b, &c, &d);
    if (a < 0x80000001) {
        return 0;
    }
    Cpuid(0x80000001, &a, &b, &c, &d);
    return (d >> 29) & 1;
}

/* ---- ACPI ----------------------------------------------------------- */

/* The RSDP is the entry point to every ACPI table: CPUs, interrupt
 * controllers, power. On BIOS systems the firmware leaves it on a
 * 16-byte boundary either in the first KiB of the EBDA or in the BIOS
 * area 0xE0000-0xFFFFF, marked by "RSD PTR " and a checksum. (UEFI
 * hands it over directly; that loader will fill the same field.) */
static int RsdpValid(const uint8_t *p)
{
    static const char Signature[8] = { 'R','S','D',' ','P','T','R',' ' };
    uint8_t Sum = 0;
    uint32_t i, Length;

    for (i = 0; i < 8; i++) {
        if (p[i] != (uint8_t)Signature[i]) {
            return 0;
        }
    }
    /* ACPI 1.0 part: the first 20 bytes sum to zero. */
    for (i = 0; i < 20; i++) {
        Sum += p[i];
    }
    if (Sum != 0) {
        return 0;
    }
    /* ACPI 2.0+ (revision >= 2): the whole structure sums to zero too. */
    if (p[15] >= 2) {
        Length = *(const uint32_t *)(p + 20);
        if (Length < 36 || Length > 64) {
            return 0;
        }
        for (Sum = 0, i = 0; i < Length; i++) {
            Sum += p[i];
        }
        if (Sum != 0) {
            return 0;
        }
    }
    return 1;
}

static uint32_t FindRsdp(void)
{
    /* The BDA keeps the EBDA's segment at 0x40E. */
    uint32_t Ebda = (uint32_t)(*(volatile uint16_t *)0x40E) << 4;
    uint32_t p;

    if (Ebda >= 0x80000 && Ebda < 0xA0000) {
        for (p = Ebda; p < Ebda + 1024; p += 16) {
            if (RsdpValid((const uint8_t *)p)) {
                return p;
            }
        }
    }
    for (p = 0xE0000; p < 0x100000; p += 16) {
        if (RsdpValid((const uint8_t *)p)) {
            return p;
        }
    }
    return 0;
}

/* ---- the boot info -------------------------------------------------- */

static void FillMemoryMap(const uint8_t *BootHeader)
{
    const E820Entry_t *Entries =
        (const E820Entry_t *)*(const uint32_t *)(BootHeader + MB_MEMMAP_ADDR);
    uint32_t Count = *(const uint32_t *)(BootHeader + MB_MEMMAP_LENGTH);
    uint32_t i, Out = 0;

    for (i = 0; i < Count && Out < MEMORY_REGIONS_MAX; i++) {
        uint32_t Type = Entries[i].Type;

        if (Entries[i].Length == 0) {
            continue;
        }
        /* Types 1-5 are passed through; anything newer or vendor-
         * specific is treated as reserved, the only safe reading. */
        if (Type < TAAJ_MEMORY_USABLE || Type > TAAJ_MEMORY_BAD) {
            Type = TAAJ_MEMORY_RESERVED;
        }
        MemoryRegions[Out].Base     = Entries[i].Base;
        MemoryRegions[Out].Length   = Entries[i].Length;
        MemoryRegions[Out].Type     = Type;
        MemoryRegions[Out].Reserved = 0;
        Out++;
    }

    g_bootInfo.MemoryMap      = (uint32_t)MemoryRegions;
    g_bootInfo.MemoryMapCount = Out;
}

static void FillFramebuffer(const uint8_t *BootHeader, const uint8_t *Vbe)
{
    TaajFramebuffer_t *Fb = &g_bootInfo.Framebuffer;

    /* No mode chosen means the screen is still in text mode, and the
     * mode-info block holds nothing meaningful. */
    if (*(const uint32_t *)(BootHeader + MB_VBE_MODE) == 0) {
        return;
    }

    Fb->Address    = *(const uint32_t *)(Vbe + VBE_FRAMEBUFFER);
    Fb->Width      = *(const uint16_t *)(Vbe + VBE_WIDTH);
    Fb->Height     = *(const uint16_t *)(Vbe + VBE_HEIGHT);
    Fb->Pitch      = *(const uint16_t *)(Vbe + VBE_PITCH);
    Fb->Bpp        = Vbe[VBE_BPP];
    Fb->RedSize    = Vbe[VBE_RED_SIZE];
    Fb->RedShift   = Vbe[VBE_RED_SIZE + 1];
    Fb->GreenSize  = Vbe[VBE_RED_SIZE + 2];
    Fb->GreenShift = Vbe[VBE_RED_SIZE + 3];
    Fb->BlueSize   = Vbe[VBE_RED_SIZE + 4];
    Fb->BlueShift  = Vbe[VBE_RED_SIZE + 5];
}

/* prepare_long_mode
 * Called from stage2.asm (cdecl) with the addresses of its MultiBoot
 * structure and VBE mode-info block. Returns the physical address of
 * the PML4 for CR3. Does not return if the CPU cannot do long mode. */
uint32_t prepare_long_mode(const uint8_t *BootHeader, const uint8_t *VbeModeInfo)
{
    if (!LongModeSupported()) {
        boot_print("[longmode] This CPU does not support 64-bit long mode.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    memsetb(&g_bootInfo, 0, sizeof(g_bootInfo));
    g_bootInfo.Magic      = TAAJ_BOOT_MAGIC;
    g_bootInfo.Version    = TAAJ_BOOT_VERSION;
    g_bootInfo.Size       = sizeof(TaajBootInfo_t);
    g_bootInfo.BootMethod = TAAJ_BOOT_BIOS;

    g_bootInfo.KernelPhysical = g_kernelAddress;
    g_bootInfo.KernelVirtual  = g_kernelVirtual;
    g_bootInfo.KernelSize     = g_kernelEnd - g_kernelAddress;

    if (g_ramdiskSize > 0) {
        g_bootInfo.RamdiskAddress = g_ramdiskAddress;
        g_bootInfo.RamdiskSize    = (uint32_t)g_ramdiskSize;
    }

    FillMemoryMap(BootHeader);
    FillFramebuffer(BootHeader, VbeModeInfo);
    g_bootInfo.AcpiRsdp   = FindRsdp();
    g_bootInfo.LoaderName = (uint32_t)LoaderName;

    BuildPageTables();
    return (uint32_t)Pml4;
}
