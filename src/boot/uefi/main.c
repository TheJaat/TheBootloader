/* main.c - TheTaaJBoot for UEFI.
 *
 * The UEFI counterpart of stage1 + stage2: it loads the same kernel ELF
 * and hands it the same TaajBootInfo, so the kernel cannot tell which
 * firmware booted it.
 *
 * UEFI does much of what the BIOS loader does by hand. The CPU is
 * already in 64-bit mode with paging on; the firmware reads FAT itself;
 * graphics, ACPI and the memory map are a function call away. What is
 * left is:
 *
 *   1. read \kernel\kernel.elf from the partition this loader came from
 *   2. copy its segments to the physical addresses it was linked for
 *   3. collect the framebuffer and the ACPI RSDP
 *   4. build page tables with the mappings the protocol promises
 *   5. take the final memory map and exit boot services - after which
 *      the firmware is gone and the machine belongs to us
 *   6. translate the memory map, switch page tables, jump
 *
 * Step 5 is the delicate one: the memory map has a "key" that changes
 * whenever memory is allocated or freed, and ExitBootServices only
 * succeeds with the current key. So nothing may allocate - not even
 * printing, which can allocate inside the firmware - between reading the
 * map and exiting. */

#include <stdint.h>
#include <stddef.h>

#include "efi.h"
#include "elf64.h"
#include "taajboot.h"

#define KERNEL_PATH         L"\\kernel\\kernel.elf"
#define LOADER_NAME         "TheTaaJBoot"   /* the kernel adds "(UEFI)" from BootMethod */

/* The protocol's kernel window: virtual KERNEL_WINDOW + x -> physical x,
 * for x below 1 GiB. The kernel is linked to run there. */
#define KERNEL_WINDOW       0xFFFFFFFF80000000ULL
#define KERNEL_WINDOW_SIZE  0x40000000ULL

#define TWO_MIB             0x200000ULL
#define ONE_GIB             0x40000000ULL
#define FOUR_GIB            0x100000000ULL

#define PAGE_PRESENT        0x001ULL
#define PAGE_WRITABLE       0x002ULL
#define PAGE_HUGE           0x080ULL

static EFI_SYSTEM_TABLE  *ST;
static EFI_BOOT_SERVICES *BS;
static EFI_HANDLE         Image;

/* ---- what a freestanding C program must supply itself --------------- */

/* The compiler may turn struct copies and zeroing loops into calls to
 * these, even with -ffreestanding, so they must exist. */
void *memset(void *Dest, int Value, size_t Count)
{
    uint8_t *D = Dest;
    while (Count--) *D++ = (uint8_t)Value;
    return Dest;
}

void *memcpy(void *Dest, const void *Source, size_t Count)
{
    uint8_t *D = Dest;
    const uint8_t *S = Source;
    while (Count--) *D++ = *S++;
    return Dest;
}

static int GuidEqual(const EFI_GUID *A, const EFI_GUID *B)
{
    const uint8_t *X = (const uint8_t *)A, *Y = (const uint8_t *)B;
    for (size_t i = 0; i < sizeof(EFI_GUID); i++) {
        if (X[i] != Y[i]) return 0;
    }
    return 1;
}

/* ---- console ---------------------------------------------------------- */

static void Print(const CHAR16 *Text)
{
    ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)Text);
}

static void PrintHex(uint64_t Value)
{
    CHAR16 Buffer[19];
    int i = 18;
    Buffer[i] = 0;
    do {
        Buffer[--i] = L"0123456789abcdef"[Value & 0xF];
        Value >>= 4;
    } while (Value && i > 2);
    Buffer[--i] = L'x';
    Buffer[--i] = L'0';
    Print(&Buffer[i]);
}

static void PrintDecimal(uint64_t Value)
{
    CHAR16 Buffer[21];
    int i = 20;
    Buffer[i] = 0;
    do {
        Buffer[--i] = (CHAR16)(L'0' + Value % 10);
        Value /= 10;
    } while (Value);
    Print(&Buffer[i]);
}

/* Reports a fatal error and gives control back to the firmware, which
 * shows its boot menu or tries the next boot option. Only usable before
 * ExitBootServices. */
#define FAIL(Message, Status) \
    do { Fail(Message, Status); return Status; } while (0)

static void Fail(const CHAR16 *Message, EFI_STATUS Status)
{
    Print(L"TheTaaJBoot: ");
    Print(Message);
    Print(L" (status ");
    PrintHex(Status);
    Print(L")\r\n");
    BS->Stall(5 * 1000 * 1000);   /* long enough to read it */
}

/* ---- loading the kernel ------------------------------------------------ */

static EFI_STATUS ReadKernel(void **Buffer, UINTN *Size)
{
    EFI_GUID LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID FileSystemGuid  = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *Loaded;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_STATUS Status;

    /* The kernel lives on the same partition as this loader. */
    Status = BS->HandleProtocol(Image, &LoadedImageGuid, (void **)&Loaded);
    if (EFI_ERROR(Status)) return Status;
    Status = BS->HandleProtocol(Loaded->DeviceHandle, &FileSystemGuid, (void **)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;

    Status = Root->Open(Root, &File, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
    Root->Close(Root);
    if (EFI_ERROR(Status)) return Status;

    /* The file size, by seeking to the end: simpler than GetInfo. */
    uint64_t End;
    File->SetPosition(File, ~0ULL);
    File->GetPosition(File, &End);
    File->SetPosition(File, 0);

    Status = BS->AllocatePool(EfiLoaderData, End, Buffer);
    if (!EFI_ERROR(Status)) {
        *Size = End;
        Status = File->Read(File, Size, *Buffer);
        if (!EFI_ERROR(Status) && *Size != End) Status = EFI_LOAD_ERROR;
    }
    File->Close(File);
    return Status;
}

/* Copies the kernel's segments to their physical addresses - the same
 * checks the BIOS loader's load_elf64 makes. */
static EFI_STATUS LoadKernel(const uint8_t *File, UINTN FileSize,
                             TaajBootInfo_t *Info, uint64_t *Entry)
{
    const Elf64_Ehdr *Header = (const Elf64_Ehdr *)File;

    if (FileSize < sizeof(Elf64_Ehdr)
        || *(const uint32_t *)Header->e_ident != ELF_MAGIC
        || Header->e_ident[4] != ELF_CLASS_64
        || Header->e_ident[5] != ELF_DATA_LSB
        || Header->e_type != ELF_TYPE_EXEC
        || Header->e_machine != ELF_MACHINE_X86_64
        || Header->e_phentsize != sizeof(Elf64_Phdr)
        || Header->e_phoff + (uint64_t)Header->e_phnum * sizeof(Elf64_Phdr) > FileSize) {
        Print(L"TheTaaJBoot: kernel.elf is not an x86_64 ELF64 executable\r\n");
        return EFI_LOAD_ERROR;
    }

    const Elf64_Phdr *Segments = (const Elf64_Phdr *)(File + Header->e_phoff);
    uint64_t Low = ~0ULL, High = 0;

    /* First pass: check every segment, and find the span they cover. */
    for (uint16_t i = 0; i < Header->e_phnum; i++) {
        const Elf64_Phdr *S = &Segments[i];
        if (S->p_type != PT_LOAD || S->p_memsz == 0) continue;
        if (S->p_vaddr != KERNEL_WINDOW + S->p_paddr
            || S->p_paddr + S->p_memsz > KERNEL_WINDOW_SIZE
            || S->p_filesz > S->p_memsz
            || S->p_offset + S->p_filesz > FileSize) {
            Print(L"TheTaaJBoot: a kernel segment is outside the kernel window\r\n");
            return EFI_LOAD_ERROR;
        }
        if (S->p_paddr < Low) Low = S->p_paddr;
        if (S->p_paddr + S->p_memsz > High) High = S->p_paddr + S->p_memsz;
    }
    if (High == 0) {
        Print(L"TheTaaJBoot: the kernel has nothing to load\r\n");
        return EFI_LOAD_ERROR;
    }

    /* Claim exactly those physical pages. On the BIOS path the loader
     * simply writes there; here the firmware owns memory and must be
     * asked, and refuses if something of its own is already there. */
    uint64_t Base = Low & ~(uint64_t)(EFI_PAGE_SIZE - 1);
    UINTN Pages = (High - Base + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Address = Base;
    EFI_STATUS Status = BS->AllocatePages(AllocateAddress, EfiLoaderData, Pages, &Address);
    if (EFI_ERROR(Status)) {
        Print(L"TheTaaJBoot: physical memory at ");
        PrintHex(Base);
        Print(L" is taken by the firmware\r\n");
        return Status;
    }

    /* Second pass: copy, and zero what the file does not hold (.bss). */
    for (uint16_t i = 0; i < Header->e_phnum; i++) {
        const Elf64_Phdr *S = &Segments[i];
        if (S->p_type != PT_LOAD || S->p_memsz == 0) continue;
        uint8_t *Target = (uint8_t *)(uintptr_t)S->p_paddr;
        memcpy(Target, File + S->p_offset, S->p_filesz);
        memset(Target + S->p_filesz, 0, S->p_memsz - S->p_filesz);
    }

    Info->KernelPhysical = Low;
    Info->KernelVirtual  = KERNEL_WINDOW + Low;
    Info->KernelSize     = High - Low;
    *Entry = Header->e_entry;
    return EFI_SUCCESS;
}

/* ---- framebuffer and ACPI ------------------------------------------------- */

/* The position and width of the run of 1 bits in a colour mask. */
static void MaskToField(uint32_t Mask, uint8_t *Size, uint8_t *Shift)
{
    uint8_t s = 0, n = 0;
    if (Mask) {
        while (!(Mask & 1)) { Mask >>= 1; s++; }
        while (Mask & 1)    { Mask >>= 1; n++; }
    }
    *Shift = s;
    *Size = n;
}

static void FindFramebuffer(TaajFramebuffer_t *Fb)
{
    EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

    if (EFI_ERROR(BS->LocateProtocol(&GopGuid, NULL, (void **)&Gop)) || !Gop->Mode) {
        return;  /* no graphics: Address stays 0 */
    }
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode = Gop->Mode->Info;

    switch (Mode->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:  /* bytes R, G, B, x */
        Fb->RedSize = Fb->GreenSize = Fb->BlueSize = 8;
        Fb->RedShift = 0; Fb->GreenShift = 8; Fb->BlueShift = 16;
        break;
    case PixelBlueGreenRedReserved8BitPerColor:  /* bytes B, G, R, x */
        Fb->RedSize = Fb->GreenSize = Fb->BlueSize = 8;
        Fb->RedShift = 16; Fb->GreenShift = 8; Fb->BlueShift = 0;
        break;
    case PixelBitMask:
        MaskToField(Mode->PixelInformation.RedMask,   &Fb->RedSize,   &Fb->RedShift);
        MaskToField(Mode->PixelInformation.GreenMask, &Fb->GreenSize, &Fb->GreenShift);
        MaskToField(Mode->PixelInformation.BlueMask,  &Fb->BlueSize,  &Fb->BlueShift);
        break;
    default:
        return;  /* PixelBltOnly: drawing only through firmware calls */
    }

    /* Every GOP linear format is 4 bytes per pixel. */
    Fb->Address = Gop->Mode->FrameBufferBase;
    Fb->Width   = Mode->HorizontalResolution;
    Fb->Height  = Mode->VerticalResolution;
    Fb->Pitch   = Mode->PixelsPerScanLine * 4;
    Fb->Bpp     = 32;
}

static uint64_t FindAcpi(void)
{
    EFI_GUID Acpi20 = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID Acpi10 = EFI_ACPI_10_TABLE_GUID;
    uint64_t Found = 0;

    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *Table = &ST->ConfigurationTable[i];
        if (GuidEqual(&Table->VendorGuid, &Acpi20)) {
            return (uint64_t)(uintptr_t)Table->VendorTable;
        }
        if (GuidEqual(&Table->VendorGuid, &Acpi10)) {
            Found = (uint64_t)(uintptr_t)Table->VendorTable;
        }
    }
    return Found;
}

/* ---- page tables ---------------------------------------------------------- */

/* Like the BIOS loader's tables, all 2 MiB pages:
 *
 *   identity       0 .. Gigabytes GiB -> the same physical addresses
 *   kernel window  KERNEL_WINDOW + x  -> x, for x below 1 GiB
 *
 * The protocol promises the first 4 GiB identity-mapped. On UEFI, RAM
 * above 4 GiB may hold things the jump still needs - this loader's own
 * code and stack - so the identity map covers all of RAM, at least 4 GiB.
 *
 * Layout of the allocation, one 4 KiB table each:
 *   [0] PML4  [1] identity PDPT  [2] window PDPT  [3] window PD
 *   [4 ..]    one identity PD per GiB */
static EFI_STATUS BuildPageTables(uint64_t Gigabytes, uint64_t *Root)
{
    UINTN Pages = 4 + Gigabytes;
    EFI_PHYSICAL_ADDRESS Address = FOUR_GIB - 1;  /* anywhere below 4 GiB */
    EFI_STATUS Status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, Pages, &Address);
    if (EFI_ERROR(Status)) return Status;

    uint64_t *Tables = (uint64_t *)(uintptr_t)Address;
    memset(Tables, 0, Pages * EFI_PAGE_SIZE);

    uint64_t *Pml4 = Tables;
    uint64_t *IdentityPdpt = Tables + 512;
    uint64_t *WindowPdpt = Tables + 2 * 512;
    uint64_t *WindowPd = Tables + 3 * 512;
    const uint64_t Table = PAGE_PRESENT | PAGE_WRITABLE;
    const uint64_t Huge  = PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;

    Pml4[0] = (uint64_t)(uintptr_t)IdentityPdpt | Table;
    for (uint64_t g = 0; g < Gigabytes; g++) {
        uint64_t *Pd = Tables + (4 + g) * 512;
        IdentityPdpt[g] = (uint64_t)(uintptr_t)Pd | Table;
        for (uint64_t j = 0; j < 512; j++) {
            Pd[j] = (g * ONE_GIB + j * TWO_MIB) | Huge;
        }
    }

    Pml4[511] = (uint64_t)(uintptr_t)WindowPdpt | Table;
    WindowPdpt[510] = (uint64_t)(uintptr_t)WindowPd | Table;
    for (uint64_t j = 0; j < 512; j++) {
        WindowPd[j] = (j * TWO_MIB) | Huge;
    }

    *Root = Address;
    return EFI_SUCCESS;
}

/* ---- the memory map -------------------------------------------------------- */

/* UEFI has 16 memory types; the kernel wants the protocol's handful. */
static uint32_t TranslateType(uint32_t EfiType)
{
    switch (EfiType) {
    case EfiConventionalMemory:
    /* Boot services memory is free once they have exited - which, when
     * the kernel reads this map, they have. */
    case EfiBootServicesCode:
    case EfiBootServicesData:
    /* This loader's own code: dead once the kernel runs. */
    case EfiLoaderCode:
        return TAAJ_MEMORY_USABLE;
    /* What this loader allocated for the kernel: the kernel image, the
     * boot info, the memory map, the page tables in use at entry. */
    case EfiLoaderData:
        return TAAJ_MEMORY_LOADER;
    case EfiACPIReclaimMemory:
        return TAAJ_MEMORY_ACPI_RECLAIMABLE;
    case EfiACPIMemoryNVS:
        return TAAJ_MEMORY_ACPI_NVS;
    case EfiUnusableMemory:
        return TAAJ_MEMORY_BAD;
    default:
        /* Reserved, runtime services (the firmware still uses those),
         * MMIO, persistent memory, and anything newer than this code. */
        return TAAJ_MEMORY_RESERVED;
    }
}

/* Converts the firmware's map into the protocol's: translated, sorted by
 * address, and with touching regions of the same type merged (a UEFI map
 * is often a hundred small pieces). Runs after ExitBootServices, so it
 * only computes - no firmware calls. Returns the number of regions. */
static uint32_t TranslateMemoryMap(const uint8_t *Map, UINTN MapSize, UINTN DescriptorSize,
                                   TaajMemoryRegion_t *Out)
{
    uint32_t Count = 0;

    for (UINTN Offset = 0; Offset + DescriptorSize <= MapSize; Offset += DescriptorSize) {
        const EFI_MEMORY_DESCRIPTOR *D = (const EFI_MEMORY_DESCRIPTOR *)(Map + Offset);
        TaajMemoryRegion_t Region = {
            .Base = D->PhysicalStart,
            .Length = D->NumberOfPages * EFI_PAGE_SIZE,
            .Type = TranslateType(D->Type),
        };
        /* Insertion sort: the map is nearly always sorted already, which
         * makes this almost linear. */
        uint32_t i = Count++;
        while (i > 0 && Out[i - 1].Base > Region.Base) {
            Out[i] = Out[i - 1];
            i--;
        }
        Out[i] = Region;
    }

    uint32_t Merged = 0;
    for (uint32_t i = 0; i < Count; i++) {
        TaajMemoryRegion_t *Last = Merged ? &Out[Merged - 1] : NULL;
        if (Last && Last->Type == Out[i].Type && Last->Base + Last->Length == Out[i].Base) {
            Last->Length += Out[i].Length;
        } else {
            Out[Merged++] = Out[i];
        }
    }
    return Merged;
}

/* ---- the jump ------------------------------------------------------------- */

/* Switches to the loader's page tables and enters the kernel, with the
 * machine state the protocol describes. Interrupts off first: the
 * firmware's interrupt handlers no longer exist. The loader's own code
 * and stack stay reachable across the CR3 load because the new tables
 * identity-map all of RAM. */
static void __attribute__((noreturn)) EnterKernel(uint64_t Root, uint64_t Entry, uint64_t Info)
{
    __asm__ volatile(
        "cli\n\t"
        "mov %0, %%cr3\n\t"
        "mov %2, %%rdi\n\t"
        "jmp *%1\n\t"
        :
        : "r"(Root), "r"(Entry), "r"(Info)
        : "rdi", "memory");
    __builtin_unreachable();
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

    Print(L"\r\nTheTaaJBoot (UEFI)\r\n");

    /* 1-2: the kernel. */
    void *File;
    UINTN FileSize;
    Status = ReadKernel(&File, &FileSize);
    if (EFI_ERROR(Status)) FAIL(L"cannot read " KERNEL_PATH, Status);

    /* The handoff block: boot info, loader name and both memory maps, in
     * one allocation below 4 GiB (the kernel reads it through the
     * identity map). Sized from the current map, plus room for the
     * entries the allocations themselves add. */
    UINTN MapSize = 0, MapKey, DescriptorSize;
    uint32_t DescriptorVersion;
    BS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    UINTN MapCapacity = MapSize + 16 * DescriptorSize;
    UINTN RegionCapacity = MapCapacity / DescriptorSize;

    const UINTN InfoBytes = 256;  /* TaajBootInfo_t, padded */
    const UINTN NameBytes = 64;
    UINTN HandoffBytes = InfoBytes + NameBytes + RegionCapacity * sizeof(TaajMemoryRegion_t) + MapCapacity;
    UINTN HandoffPages = (HandoffBytes + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Handoff = FOUR_GIB - 1;
    Status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, HandoffPages, &Handoff);
    if (EFI_ERROR(Status)) FAIL(L"no memory for the boot info", Status);
    memset((void *)(uintptr_t)Handoff, 0, HandoffPages * EFI_PAGE_SIZE);

    TaajBootInfo_t *Info = (TaajBootInfo_t *)(uintptr_t)Handoff;
    char *Name = (char *)(uintptr_t)(Handoff + InfoBytes);
    TaajMemoryRegion_t *Regions = (TaajMemoryRegion_t *)(uintptr_t)(Handoff + InfoBytes + NameBytes);
    uint8_t *EfiMap = (uint8_t *)(Regions + RegionCapacity);

    uint64_t Entry;
    Status = LoadKernel(File, FileSize, Info, &Entry);
    if (EFI_ERROR(Status)) FAIL(L"cannot load the kernel", Status);
    BS->FreePool(File);

    /* 3: everything else the kernel wants to know. */
    Info->Magic      = TAAJ_BOOT_MAGIC;
    Info->Version    = TAAJ_BOOT_VERSION;
    Info->Size       = sizeof(TaajBootInfo_t);
    Info->BootMethod = TAAJ_BOOT_UEFI;
    Info->AcpiRsdp   = FindAcpi();
    FindFramebuffer(&Info->Framebuffer);
    memcpy(Name, LOADER_NAME, sizeof(LOADER_NAME));
    Info->LoaderName = (uint64_t)(uintptr_t)Name;

    /* 4: page tables, sized to cover all of RAM. */
    MapSize = MapCapacity;
    Status = BS->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR *)EfiMap, &MapKey,
                              &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) FAIL(L"cannot read the memory map", Status);
    uint64_t TopOfRam = FOUR_GIB;
    for (UINTN Offset = 0; Offset + DescriptorSize <= MapSize; Offset += DescriptorSize) {
        EFI_MEMORY_DESCRIPTOR *D = (EFI_MEMORY_DESCRIPTOR *)(EfiMap + Offset);
        int IsRam = D->Type != EfiReservedMemoryType && D->Type != EfiMemoryMappedIO
                    && D->Type != EfiMemoryMappedIOPortSpace;
        uint64_t End = D->PhysicalStart + D->NumberOfPages * EFI_PAGE_SIZE;
        if (IsRam && End > TopOfRam) TopOfRam = End;
    }
    uint64_t Gigabytes = (TopOfRam + ONE_GIB - 1) / ONE_GIB;
    if (Gigabytes > 512) Gigabytes = 512;  /* one PDPT's worth */

    uint64_t Root;
    Status = BuildPageTables(Gigabytes, &Root);
    if (EFI_ERROR(Status)) FAIL(L"no memory for page tables", Status);

    Print(L"  kernel ");
    PrintDecimal(Info->KernelSize / 1024);
    Print(L" KiB at ");
    PrintHex(Info->KernelPhysical);
    Print(L", entry ");
    PrintHex(Entry);
    Print(L"\r\n  framebuffer ");
    PrintDecimal(Info->Framebuffer.Width);
    Print(L"x");
    PrintDecimal(Info->Framebuffer.Height);
    Print(L" at ");
    PrintHex(Info->Framebuffer.Address);
    Print(L", ACPI RSDP ");
    PrintHex(Info->AcpiRsdp);
    Print(L"\r\n  identity map ");
    PrintDecimal(Gigabytes);
    Print(L" GiB - exiting boot services\r\n");

    /* 5: the final map, then exit. If the key is stale (something
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
            for (;;) __asm__ volatile("cli; hlt");
        }
    }

    /* 6: no firmware from here on. Nothing may call BS or ST. */
    Info->MemoryMap = (uint64_t)(uintptr_t)Regions;
    Info->MemoryMapCount = TranslateMemoryMap(EfiMap, MapSize, DescriptorSize, Regions);

    EnterKernel(Root, Entry, (uint64_t)(uintptr_t)Info);
}
