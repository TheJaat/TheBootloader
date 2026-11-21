/* aarch64.c - TheTaaJBoot for UEFI on AArch64, as BOOTAA64.EFI.
 *
 * The AArch64 kernel is not started the way the x86_64 one is. It is a
 * flat image, entered at its first byte with the MMU off and the
 * address of a device tree in x0 - the way every AArch64 loader starts
 * a kernel (QEMU's -kernel, U-Boot, the Raspberry Pi's firmware), so
 * the kernel needs no loader of its own to run. This is one more such
 * loader, for machines whose firmware is UEFI:
 *
 *   1. read \kernel\Image from the partition this loader came from
 *   2. put it at an address of the kind its header asks for
 *   3. find the device tree the firmware has
 *   4. take the final memory map and exit boot services
 *   5. write a copy of the device tree that also lists the memory the
 *      firmware keeps for itself
 *   6. turn the MMU off and jump
 *
 * Step 5 is what a UEFI machine adds. The device tree says where the
 * RAM is - all of it. But some of it the firmware still uses after it
 * has handed over (its runtime services live there), and only the
 * firmware's memory map says which. The kernel does not read that map;
 * it reads the tree. So the map's "keep out" ranges are written into
 * the tree's own list of reserved memory, which the kernel already
 * honours. The kernel stays ignorant of UEFI.
 *
 * Step 6 undoes what UEFI did for us. Firmware runs its applications
 * with the MMU and the caches on, every address mapped to itself. The
 * kernel's first instructions expect them off. Turning a data cache
 * off loses whatever is in it and not yet in memory, so what the
 * kernel will read - its image, the tree - is first written through to
 * memory (CleanCaches). */

#include <stdint.h>
#include <stddef.h>

#include "efi.h"
#include "common.h"

#define KERNEL_PATH         L"\\kernel\\Image"

#define TWO_MIB             0x200000ULL

/* ---- the kernel image ---------------------------------------------------- */

/* The first 64 bytes of an AArch64 kernel image, as Linux defined them
 * and every loader expects them. All little-endian. */
typedef struct {
    uint32_t Code0;             /* an instruction: a jump over this header */
    uint32_t Code1;
    uint64_t TextOffset;        /* how far into a 2 MiB step to load it     */
    uint64_t ImageSize;         /* memory it needs, .bss included           */
    uint64_t Flags;
    uint64_t Reserved[3];
    uint32_t Magic;             /* "ARM\x64"                                */
    uint32_t Reserved2;
} ImageHeader;

#define IMAGE_MAGIC         0x644D5241U

/* Copies the image to memory of its own. *Base is where its first byte
 * went - which is also where it is entered. */
static EFI_STATUS LoadImage(const uint8_t *File, UINTN FileSize, uint64_t *Base, uint64_t *Size)
{
    const ImageHeader *Header = (const ImageHeader *)File;

    if (FileSize < sizeof(ImageHeader) || Header->Magic != IMAGE_MAGIC) {
        Print(L"TheTaaJBoot: Image is not an AArch64 kernel image\r\n");
        return EFI_LOAD_ERROR;
    }
    if (Header->ImageSize < FileSize || Header->TextOffset >= TWO_MIB) {
        Print(L"TheTaaJBoot: the image's header does not describe the file it is in\r\n");
        return EFI_LOAD_ERROR;
    }

    /* The header asks for an address that is TextOffset past a multiple
     * of 2 MiB. The firmware gives out memory by the page, wherever it
     * likes; so ask for 4 MiB more than is needed, and such an address
     * is in there somewhere. */
    UINTN Pages = (Header->ImageSize + 2 * TWO_MIB + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Address = 0;
    EFI_STATUS Status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &Address);
    if (EFI_ERROR(Status)) {
        Print(L"TheTaaJBoot: no memory for the kernel\r\n");
        return Status;
    }
    uint64_t Start = ((Address + TWO_MIB - 1) & ~(TWO_MIB - 1)) + Header->TextOffset;

    memcpy((void *)(uintptr_t)Start, File, FileSize);
    memset((uint8_t *)(uintptr_t)Start + FileSize, 0, Header->ImageSize - FileSize);

    *Base = Start;
    *Size = Header->ImageSize;
    return EFI_SUCCESS;
}

/* ---- the device tree ------------------------------------------------------- */

/* A device tree blob is big-endian throughout. */
static uint32_t Be32(const uint8_t *P)
{
    return ((uint32_t)P[0] << 24) | ((uint32_t)P[1] << 16) | ((uint32_t)P[2] << 8) | P[3];
}

static void PutBe32(uint8_t *P, uint32_t Value)
{
    P[0] = (uint8_t)(Value >> 24); P[1] = (uint8_t)(Value >> 16); P[2] = (uint8_t)(Value >> 8); P[3] = (uint8_t)Value;
}

static uint64_t Be64(const uint8_t *P)
{
    return ((uint64_t)Be32(P) << 32) | Be32(P + 4);
}

static void PutBe64(uint8_t *P, uint64_t Value)
{
    PutBe32(P, (uint32_t)(Value >> 32));
    PutBe32(P + 4, (uint32_t)Value);
}

#define FDT_MAGIC           0xD00DFEEDU
#define FDT_HEADER_SIZE     40
/* Offsets in the header. */
#define FDT_TOTAL_SIZE      4
#define FDT_STRUCT_OFFSET   8
#define FDT_STRINGS_OFFSET  12
#define FDT_RESERVED_OFFSET 16
#define FDT_STRINGS_SIZE    32
#define FDT_STRUCT_SIZE     36

/* The device tree the firmware has: one of its configuration tables.
 * NULL if it has none - or what it has is not one. */
static const uint8_t *FindDeviceTree(uint32_t *Size)
{
    EFI_GUID Dtb = EFI_DTB_TABLE_GUID;

    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *Table = &ST->ConfigurationTable[i];
        if (!GuidEqual(&Table->VendorGuid, &Dtb)) continue;
        const uint8_t *Tree = Table->VendorTable;
        if (Be32(Tree) != FDT_MAGIC) return NULL;
        uint32_t Total = Be32(Tree + FDT_TOTAL_SIZE);
        /* Every part must lie inside the blob: this loader is about to
         * copy them by the sizes the header gives. */
        uint64_t Struct = Be32(Tree + FDT_STRUCT_OFFSET), StructSize = Be32(Tree + FDT_STRUCT_SIZE);
        uint64_t Strings = Be32(Tree + FDT_STRINGS_OFFSET), StringsSize = Be32(Tree + FDT_STRINGS_SIZE);
        uint64_t Reserved = Be32(Tree + FDT_RESERVED_OFFSET);
        if (Total < FDT_HEADER_SIZE || Struct + StructSize > Total || Strings + StringsSize > Total
            || Reserved < FDT_HEADER_SIZE || Reserved + 16 > Total) {
            return NULL;
        }
        *Size = Total;
        return Tree;
    }
    return NULL;
}

/* One range of memory. */
typedef struct {
    uint64_t Base;
    uint64_t Length;
} Range;

/* Is memory of this type something the kernel must stay out of?
 *
 * Not: free memory; the boot services' memory (they have exited when
 * the kernel reads this); and this loader's own - its code is dead
 * once the kernel runs, and its data is the kernel image and the tree,
 * which the kernel knows the places of itself.
 *
 * Not either: device registers. They are not RAM, the tree's memory
 * node does not cover them, and there is nothing to keep out of. */
static int KeepOut(uint32_t EfiType)
{
    switch (EfiType) {
    case EfiConventionalMemory:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
        return 0;
    default:
        /* The firmware's runtime services, ACPI tables, bad memory,
         * "reserved", and any type newer than this code. */
        return 1;
    }
}

/* The firmware's memory map as a list of ranges to keep out of: sorted
 * by address, touching ones made one. Returns how many. Only computes:
 * it runs after ExitBootServices too. */
static uint32_t KeepOutRanges(const uint8_t *Map, UINTN MapSize, UINTN DescriptorSize, Range *Out)
{
    uint32_t Count = 0;

    for (UINTN Offset = 0; Offset + DescriptorSize <= MapSize; Offset += DescriptorSize) {
        const EFI_MEMORY_DESCRIPTOR *D = (const EFI_MEMORY_DESCRIPTOR *)(Map + Offset);
        if (!KeepOut(D->Type) || D->NumberOfPages == 0) continue;
        Range New = { D->PhysicalStart, D->NumberOfPages * EFI_PAGE_SIZE };
        /* Insertion sort: the map is nearly always sorted already. */
        uint32_t i = Count++;
        while (i > 0 && Out[i - 1].Base > New.Base) {
            Out[i] = Out[i - 1];
            i--;
        }
        Out[i] = New;
    }

    uint32_t Merged = 0;
    for (uint32_t i = 0; i < Count; i++) {
        Range *Last = Merged ? &Out[Merged - 1] : NULL;
        if (Last && Last->Base + Last->Length == Out[i].Base) {
            Last->Length += Out[i].Length;
        } else {
            Out[Merged++] = Out[i];
        }
    }
    return Merged;
}

/* Writes at `New` a copy of the tree `Old` whose list of reserved
 * memory has `Count` more ranges. Returns the copy's size.
 *
 * A blob is four parts - header, reserved list, structure, strings -
 * and the header says where each begins. The list is not at the end,
 * so it cannot simply grow: the parts after it are written further
 * along, and the header is told. Nothing inside the structure refers
 * to a position in the blob, so nothing else changes.
 *
 * The caller has made room: the old size plus 16 bytes a range, and 16
 * to spare for rounding. */
static uint32_t WriteDeviceTree(uint8_t *New, const uint8_t *Old, const Range *Ranges, uint32_t Count)
{
    uint32_t Struct = Be32(Old + FDT_STRUCT_OFFSET), StructSize = Be32(Old + FDT_STRUCT_SIZE);
    uint32_t Strings = Be32(Old + FDT_STRINGS_OFFSET), StringsSize = Be32(Old + FDT_STRINGS_SIZE);
    uint32_t Total = Be32(Old + FDT_TOTAL_SIZE);

    memcpy(New, Old, FDT_HEADER_SIZE);

    /* The list: what was there (it ends with a pair of zeroes), then
     * ours, then the end again. */
    uint32_t At = FDT_HEADER_SIZE;
    for (uint32_t From = Be32(Old + FDT_RESERVED_OFFSET); From + 16 <= Total; From += 16) {
        if (Be64(Old + From) == 0 && Be64(Old + From + 8) == 0) break;
        memcpy(New + At, Old + From, 16);
        At += 16;
    }
    for (uint32_t i = 0; i < Count; i++) {
        PutBe64(New + At, Ranges[i].Base);
        PutBe64(New + At + 8, Ranges[i].Length);
        At += 16;
    }
    PutBe64(New + At, 0);
    PutBe64(New + At + 8, 0);
    At += 16;

    /* The structure (it must begin on a multiple of 4; the list's end
     * is a multiple of 8), then the strings. */
    PutBe32(New + FDT_RESERVED_OFFSET, FDT_HEADER_SIZE);
    PutBe32(New + FDT_STRUCT_OFFSET, At);
    memcpy(New + At, Old + Struct, StructSize);
    At += StructSize;
    PutBe32(New + FDT_STRINGS_OFFSET, At);
    memcpy(New + At, Old + Strings, StringsSize);
    At += StringsSize;
    PutBe32(New + FDT_TOTAL_SIZE, At);
    return At;
}

/* ---- the jump --------------------------------------------------------------- */

/* Makes memory hold what the caches hold, from Start up to End, and
 * the instruction cache forget what it had from there.
 *
 * The kernel is about to run with the caches off. What this loader
 * wrote - the image, the tree - may so far be only in the data cache:
 * turned off, the cache is no longer looked in, and the kernel would
 * read whatever was in memory before. "dc cvac" writes one cache line
 * through; CTR_EL0 says how long a line is. And where the image now is
 * something else may have run before (the firmware's own code, since
 * freed), which the instruction cache could still hold: "ic ivau". */
static void CleanCaches(uint64_t Start, uint64_t End)
{
    uint64_t Type;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(Type));
    /* Bits 16-19: the data cache's line, as a power of two of words. */
    uint64_t Line = 4ULL << ((Type >> 16) & 0xF);
    for (uint64_t At = Start & ~(Line - 1); At < End; At += Line) {
        __asm__ volatile("dc cvac, %0" : : "r"(At));
    }
    __asm__ volatile("dsb sy");
    /* Bits 0-3: the instruction cache's, likewise. */
    Line = 4ULL << (Type & 0xF);
    for (uint64_t At = Start & ~(Line - 1); At < End; At += Line) {
        __asm__ volatile("ic ivau, %0" : : "r"(At));
    }
    __asm__ volatile("dsb sy\n\tisb");
}

/* Enters the kernel at `Entry` the way it expects: interrupts off, the
 * MMU and the data cache off, the device tree's address in x0 and
 * nothing in x1 to x3.
 *
 * Assembly, and no stack: once the data cache is off, what was written
 * to the stack a moment ago may not be what a read finds there. The
 * instructions themselves go on running where they are - the firmware
 * had every address mapped to itself, so with the MMU off the next
 * instruction is at the same address as before.
 *
 * The firmware may have been running at exception level 1 or at 2
 * (when the machine can run virtual machines); each level has its own
 * control register, and the kernel copes with being entered at either. */
__attribute__((naked, noreturn)) static void EnterKernel(uint64_t Entry, uint64_t Tree)
{
    __asm__(
        "msr    daifset, #0xf\n\t"      /* no interrupts of any kind */
        "mov    x10, #5\n\t"            /* the MMU (bit 0) and the data cache (bit 2) */
        "mrs    x9, CurrentEL\n\t"
        "cmp    x9, #8\n\t"             /* level 2? (the level is in bits 2-3) */
        "b.eq   1f\n\t"
        "mrs    x9, sctlr_el1\n\t"
        "bic    x9, x9, x10\n\t"
        "msr    sctlr_el1, x9\n\t"
        "b      2f\n\t"
        "1:\n\t"
        "mrs    x9, sctlr_el2\n\t"
        "bic    x9, x9, x10\n\t"
        "msr    sctlr_el2, x9\n\t"
        "2:\n\t"
        "isb\n\t"
        "mov    x4, x0\n\t"
        "mov    x0, x1\n\t"
        "mov    x1, xzr\n\t"
        "mov    x2, xzr\n\t"
        "mov    x3, xzr\n\t"
        "br     x4\n\t");
}

static uint64_t ExceptionLevel(void)
{
    uint64_t Level;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(Level));
    return (Level >> 2) & 3;
}

/* ---- the entry point ---------------------------------------------------- */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    ST = SystemTable;
    BS = SystemTable->BootServices;
    Image = ImageHandle;

    /* The firmware reboots the machine if a boot application runs for
     * five minutes. This one will run forever - as the OS. */
    BS->SetWatchdogTimer(0, 0, 0, NULL);

    Print(L"\r\nTheTaaJBoot (UEFI, AArch64)\r\n");

    /* 1-2: the kernel. */
    void *File;
    UINTN FileSize;
    Status = ReadFile(KERNEL_PATH, &File, &FileSize);
    if (EFI_ERROR(Status)) FAIL(L"cannot read " KERNEL_PATH, Status);
    uint64_t Kernel, KernelSize;
    Status = LoadImage(File, FileSize, &Kernel, &KernelSize);
    if (EFI_ERROR(Status)) FAIL(L"cannot load the kernel", Status);
    BS->FreePool(File);

    /* 3: the device tree. Firmware that describes the machine with
     * ACPI tables instead has none to give, and this kernel reads no
     * ACPI on AArch64. */
    uint32_t TreeSize = 0;
    const uint8_t *Tree = FindDeviceTree(&TreeSize);
    if (!Tree) {
        FAIL(L"the firmware has no device tree to give (on QEMU: start the machine with -M virt,acpi=off)", EFI_NOT_FOUND);
    }

    /* Memory for the firmware's map, for the ranges made of it, and for
     * the new tree - all sized from the map as it is now, plus room for
     * the entries these allocations themselves add. Everything is
     * allocated before the final map is read: after that nothing may. */
    UINTN MapSize = 0, MapKey, DescriptorSize;
    uint32_t DescriptorVersion;
    BS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    UINTN MapCapacity = MapSize + 16 * DescriptorSize;
    UINTN RangeCapacity = MapCapacity / DescriptorSize;

    uint8_t *EfiMap;
    Range *Ranges;
    Status = BS->AllocatePool(EfiLoaderData, MapCapacity, (void **)&EfiMap);
    if (EFI_ERROR(Status)) FAIL(L"no memory for the memory map", Status);
    Status = BS->AllocatePool(EfiLoaderData, RangeCapacity * sizeof(Range), (void **)&Ranges);
    if (EFI_ERROR(Status)) FAIL(L"no memory for the memory map", Status);

    UINTN NewTreeCapacity = TreeSize + RangeCapacity * 16 + 16;
    UINTN NewTreePages = (NewTreeCapacity + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS NewTree = 0;
    Status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, NewTreePages, &NewTree);
    if (EFI_ERROR(Status)) FAIL(L"no memory for the device tree", Status);

    /* What is about to happen, said while saying is still allowed. The
     * count of ranges is the map's as it is now; the final one is read
     * below, and the kernel prints what it was given. */
    MapSize = MapCapacity;
    Status = BS->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR *)EfiMap, &MapKey,
                              &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) FAIL(L"cannot read the memory map", Status);
    uint32_t Count = KeepOutRanges(EfiMap, MapSize, DescriptorSize, Ranges);

    Print(L"  kernel ");
    PrintDecimal(KernelSize / 1024);
    Print(L" KiB at ");
    PrintHex(Kernel);
    Print(L"\r\n  device tree ");
    PrintDecimal(TreeSize);
    Print(L" bytes at ");
    PrintHex((uint64_t)(uintptr_t)Tree);
    Print(L", copied to ");
    PrintHex(NewTree);
    Print(L" with ");
    PrintDecimal(Count);
    Print(L" ranges the firmware keeps\r\n  firmware at exception level ");
    PrintDecimal(ExceptionLevel());
    Print(L" - exiting boot services\r\n");

    /* 4: the final map, then exit. If the key is stale (something
     * allocated in between - firmware timers can), the spec allows only
     * GetMemoryMap and ExitBootServices to be called again. */
    for (int Attempt = 0; ; Attempt++) {
        MapSize = MapCapacity;
        Status = BS->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR *)EfiMap, &MapKey,
                                  &DescriptorSize, &DescriptorVersion);
        if (!EFI_ERROR(Status)) {
            Status = BS->ExitBootServices(Image, MapKey);
            if (!EFI_ERROR(Status)) break;
        }
        if (Attempt == 3) {
            /* Not even printing is allowed now; all that is left is to
             * stop. */
            for (;;) __asm__ volatile("msr daifset, #0xf\n\twfi");
        }
    }

    /* 5: no firmware from here on. Nothing may call BS or ST. */
    Count = KeepOutRanges(EfiMap, MapSize, DescriptorSize, Ranges);
    uint32_t NewTreeSize = WriteDeviceTree((uint8_t *)(uintptr_t)NewTree, Tree, Ranges, Count);

    /* 6. */
    CleanCaches(Kernel, Kernel + KernelSize);
    CleanCaches(NewTree, NewTree + NewTreeSize);
    EnterKernel(Kernel, NewTree);
}
