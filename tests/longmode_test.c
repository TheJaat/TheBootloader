/* longmode_test.c - what stage 2 builds for a 64-bit kernel
 * (longmode.c): the page tables, walked here as the CPU walks them; the
 * memory map and the framebuffer it hands over; the RSDP checksum.
 *
 * longmode.c's functions are static - each is one step of
 * prepare_long_mode, which also reads the BIOS data area at 0x40E, the
 * one address a test cannot map (harness.h) - so the file is included
 * here, whole, and its steps called one by one. */

#include "harness.h"
#include "../src/boot/stage2/longmode.c"

/* ---- the CPU's walk --------------------------------------------------- */

#define NOT_MAPPED 0xFFFFFFFFFFFFFFFFULL
#define ADDRESS_BITS 0x000FFFFFFFFFF000ULL

/* The physical address the tables map `virtual` to, as an x86_64 CPU
 * finds it - four levels, 9 bits each, a 2 MiB page ending the walk at
 * the third - or NOT_MAPPED. Each level's table is found by the address
 * in the entry above, which here, in a 32-bit process with stage 2's
 * tables at their own addresses, is a pointer. A non-canonical address
 * (bits 63-48 not all equal to bit 47) is never mapped. */
static uint64_t walk(uint64_t virtual)
{
    uint64_t top = virtual >> 47;
    if (top != 0 && top != 0x1FFFF) {
        return NOT_MAPPED;
    }
    const uint64_t *table = Pml4;
    for (int level = 3; level >= 1; level--) {
        uint64_t entry = table[(virtual >> (12 + 9 * level)) & 511];
        if (!(entry & PAGE_PRESENT)) {
            return NOT_MAPPED;
        }
        if (level == 1 && (entry & PAGE_HUGE)) {
            return (entry & ADDRESS_BITS & ~(TWO_MIB - 1)) | (virtual & (TWO_MIB - 1));
        }
        if (level == 1) {
            return NOT_MAPPED; /* stage 2 makes no 4 KiB pages */
        }
        table = (const uint64_t *)(uint32_t)(entry & ADDRESS_BITS);
    }
    return NOT_MAPPED;
}

static void the_page_tables_map_what_the_kernel_needs(void)
{
    /* .bss in the flat binary is whatever was in memory: garbage first. */
    set_bytes(Pml4, 0x5A, sizeof Pml4);
    set_bytes(IdentityPdpt, 0x5A, sizeof IdentityPdpt);
    set_bytes(IdentityPd, 0x5A, sizeof IdentityPd);
    set_bytes(WindowPdpt, 0x5A, sizeof WindowPdpt);
    set_bytes(WindowPd, 0x5A, sizeof WindowPd);
    BuildPageTables();

    CHECK(((uint32_t)Pml4 & 4095) == 0 && ((uint32_t)IdentityPdpt & 4095) == 0 && ((uint32_t)IdentityPd & 4095) == 0
              && ((uint32_t)WindowPdpt & 4095) == 0 && ((uint32_t)WindowPd & 4095) == 0,
          "every table on a 4 KiB boundary: the CPU ignores an entry's low 12 bits");

    /* Identity: 0 to 4 GiB. */
    CHECK(walk(0) == 0, "address 0, identity");
    CHECK(walk(0x9000) == 0x9000, "stage 2 itself, identity: its next instruction after paging is on");
    CHECK(walk(0x123456) == 0x123456, "inside a 2 MiB page: the low 21 bits carried over");
    CHECK(walk(0xFD000000) == 0xFD000000, "a framebuffer high below 4 GiB");
    CHECK(walk(0xFFFFFFFF) == 0xFFFFFFFF, "the last byte below 4 GiB");
    CHECK(walk(0x100000000ULL) == NOT_MAPPED, "4 GiB: not mapped");

    /* The kernel window: 0xFFFFFFFF80000000 + x is x, for x below 1 GiB. */
    CHECK(walk(0xFFFFFFFF80000000ULL) == 0, "the window's start is physical 0");
    CHECK(walk(0xFFFFFFFF80100000ULL) == 0x100000, "a kernel linked at the window's 1 MiB");
    CHECK(walk(0xFFFFFFFFBFFFFFFFULL) == 0x3FFFFFFF, "the window's last byte");
    CHECK(walk(0xFFFFFFFFC0000000ULL) == NOT_MAPPED, "past the window's 1 GiB: not mapped");
    CHECK(walk(0xFFFF800000000000ULL) == NOT_MAPPED, "the start of the upper half: not mapped");

    CHECK(walk(0x0000800000000000ULL) == NOT_MAPPED, "non-canonical: never mapped");

    /* Nothing else: every entry not set is zero, not garbage. */
    int others_zero = 1;
    for (int i = 1; i < 511; i++) {
        others_zero &= Pml4[i] == 0;
    }
    for (int i = 4; i < 512; i++) {
        others_zero &= IdentityPdpt[i] == 0;
    }
    for (int i = 0; i < 510; i++) {
        others_zero &= WindowPdpt[i] == 0;
    }
    others_zero &= WindowPdpt[511] == 0;
    CHECK(others_zero, "every other entry zero: the garbage in .bss is cleared");

    int writable = 1;
    for (int j = 0; j < 512; j++) {
        writable &= (WindowPd[j] & (PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE)) == (PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE);
    }
    CHECK(writable, "the window's pages: present, writable, 2 MiB");
}

/* ---- the memory map ------------------------------------------------------ */

/* The MultiBoot block stage2.asm fills in real mode, as longmode.c
 * reads it, and the E820 entries it points to. */
static uint8_t boot_header[0x60];
static E820Entry_t entries[80];

static void set_e820(uint32_t count)
{
    set_bytes(boot_header, 0, sizeof boot_header);
    *(uint32_t *)(boot_header + MB_MEMMAP_ADDR) = (uint32_t)entries;
    *(uint32_t *)(boot_header + MB_MEMMAP_LENGTH) = count;
}

static void the_memory_map_is_handed_over(void)
{
    set_bytes(entries, 0, sizeof entries);
    entries[0] = (E820Entry_t){ .Base = 0, .Length = 0x9FC00, .Type = 1 };
    entries[1] = (E820Entry_t){ .Base = 0x9FC00, .Length = 0, .Type = 2 };          /* empty: dropped */
    entries[2] = (E820Entry_t){ .Base = 0x100000, .Length = 0x7EE0000, .Type = 1 };
    entries[3] = (E820Entry_t){ .Base = 0xFEC00000, .Length = 0x1000, .Type = 0 };  /* not a type */
    entries[4] = (E820Entry_t){ .Base = 0x100000000ULL, .Length = 0x40000000, .Type = 1 }; /* above 4 GiB */
    entries[5] = (E820Entry_t){ .Base = 0xFED00000, .Length = 0x1000, .Type = 12 }; /* persistent memory */
    entries[6] = (E820Entry_t){ .Base = 0x7FE0000, .Length = 0x20000, .Type = 3 };
    set_e820(7);
    memsetb(&g_bootInfo, 0, sizeof g_bootInfo);
    FillMemoryMap(boot_header);

    TaajMemoryRegion_t *out = (TaajMemoryRegion_t *)(uint32_t)g_bootInfo.MemoryMap;
    CHECK(g_bootInfo.MemoryMapCount == 6, "seven entries, the empty one dropped");
    CHECK(out == MemoryRegions, "the map's address, physical (a global's address in stage 2)");
    CHECK(out[0].Base == 0 && out[0].Length == 0x9FC00 && out[0].Type == TAAJ_MEMORY_USABLE, "the first, as it was");
    CHECK(out[2].Type == TAAJ_MEMORY_RESERVED, "type 0, not a type: reserved, the only safe reading");
    CHECK(out[3].Base == 0x100000000ULL && out[3].Length == 0x40000000, "memory above 4 GiB, its 64 bits kept");
    CHECK(out[4].Type == TAAJ_MEMORY_RESERVED, "type 12, newer than the protocol: reserved");
    CHECK(out[5].Type == TAAJ_MEMORY_ACPI_RECLAIMABLE, "types 1-5 passed through");

    for (int i = 0; i < 80; i++) {
        entries[i] = (E820Entry_t){ .Base = (uint64_t)i << 20, .Length = 0x1000, .Type = 1 };
    }
    set_e820(80);
    FillMemoryMap(boot_header);
    CHECK(g_bootInfo.MemoryMapCount == MEMORY_REGIONS_MAX, "80 entries: the first 64 kept, the table not overrun");
}

static void the_framebuffer_is_handed_over(void)
{
    uint8_t vbe[256];
    set_bytes(vbe, 0, sizeof vbe);
    *(uint32_t *)(vbe + VBE_FRAMEBUFFER) = 0xFD000000;
    *(uint16_t *)(vbe + VBE_WIDTH) = 1024;
    *(uint16_t *)(vbe + VBE_HEIGHT) = 768;
    *(uint16_t *)(vbe + VBE_PITCH) = 4096;
    vbe[VBE_BPP] = 32;
    vbe[VBE_RED_SIZE] = 8;  vbe[VBE_RED_SIZE + 1] = 16;
    vbe[VBE_RED_SIZE + 2] = 8;  vbe[VBE_RED_SIZE + 3] = 8;
    vbe[VBE_RED_SIZE + 4] = 8;  vbe[VBE_RED_SIZE + 5] = 0;

    set_e820(0);
    memsetb(&g_bootInfo, 0, sizeof g_bootInfo);
    FillFramebuffer(boot_header, vbe);
    CHECK(g_bootInfo.Framebuffer.Address == 0, "no mode chosen (text mode): no framebuffer, whatever the block says");

    *(uint32_t *)(boot_header + MB_VBE_MODE) = 0x118;
    FillFramebuffer(boot_header, vbe);
    TaajFramebuffer_t *fb = &g_bootInfo.Framebuffer;
    CHECK(fb->Address == 0xFD000000 && fb->Width == 1024 && fb->Height == 768, "address and size");
    CHECK(fb->Pitch == 4096, "the pitch from the block - not width times four");
    CHECK(fb->Bpp == 32 && fb->RedShift == 16 && fb->GreenShift == 8 && fb->BlueShift == 0 && fb->RedSize == 8,
          "the colour layout");
}

/* ---- the RSDP ------------------------------------------------------------ */

static void sum_to_zero(uint8_t *bytes, uint32_t length, uint32_t at)
{
    uint8_t sum = 0;
    bytes[at] = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    bytes[at] = (uint8_t)-sum;
}

static void the_rsdp_checksums(void)
{
    uint8_t rsdp[64];
    set_bytes(rsdp, 0, sizeof rsdp);
    copy_bytes(rsdp, "RSD PTR ", 8);
    rsdp[15] = 0;                     /* ACPI 1.0 */
    sum_to_zero(rsdp, 20, 8);
    CHECK(RsdpValid(rsdp), "a good ACPI 1.0 RSDP");
    rsdp[9] ^= 1;
    CHECK(!RsdpValid(rsdp), "one byte changed: its checksum fails");
    rsdp[9] ^= 1;
    rsdp[0] = 'r';
    CHECK(!RsdpValid(rsdp), "the signature wrong");
    rsdp[0] = 'R';

    rsdp[15] = 2;                     /* ACPI 2.0: 36 bytes, a second sum */
    *(uint32_t *)(rsdp + 20) = 36;
    sum_to_zero(rsdp, 20, 8);
    sum_to_zero(rsdp, 36, 32);
    CHECK(RsdpValid(rsdp), "a good ACPI 2.0 RSDP");
    rsdp[30] ^= 1;
    sum_to_zero(rsdp, 20, 8);         /* the first part still fine */
    CHECK(!RsdpValid(rsdp), "the extended checksum fails");
    *(uint32_t *)(rsdp + 20) = 1000;
    sum_to_zero(rsdp, 20, 8);
    CHECK(!RsdpValid(rsdp), "a length past any RSDP: refused before it is summed");
}

static void this_cpu_can_run_long_mode(void)
{
    CHECK(LongModeSupported(), "CPUID finds long mode on the machine running the tests");
}

void longmode_tests(void)
{
    the_page_tables_map_what_the_kernel_needs();
    the_memory_map_is_handed_over();
    the_framebuffer_is_handed_over();
    the_rsdp_checksums();
    this_cpu_can_run_long_mode();
}
