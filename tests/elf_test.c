/* elf_test.c - the kernel loader (elf.c), on ELF files made here at the
 * staging address 0x300000, as load_kernel leaves the real one - good
 * ones, and each thing the loader must refuse, by its message - and on
 * the build's own 64-bit kernel, if one is given. */

#include "harness.h"
#include <elf.h>

int load_elf(void);
int usable_memory(unsigned long long base, unsigned long long end);
extern unsigned char g_kernelHead[64];

extern unsigned int g_kernelAddress, g_kernelEnd, g_kernelEntry;
extern unsigned int g_kernelIs64;
extern unsigned long long g_kernelEntry64, g_kernelVirtual;

#define STAGED 0x300000u                   /* elf.c's KERNEL_ELF_READ */
#define WINDOW 0xFFFFFFFF80000000ULL       /* the kernel window */

static uint8_t *const staged = (uint8_t *)STAGED;

/* ---- loading, as stage 2 does it (K130) ------------------------------------ */

/* A 64-bit kernel's file is read where it lies, through kernel_read; a
 * 32-bit one is staged at 0x300000. The tests make each in `file`. */
static uint8_t *file;

/* What load_kernel (vfs.c) does before load_elf: the file's first 64
 * bytes into g_kernelHead - and for a 32-bit kernel, the whole file at
 * 0x300000. */
static int load(void)
{
    copy_bytes(g_kernelHead, file, 64);
    kernel_file = file;
    kernel_file_size = (uint32_t)g_kernelSize;
    kernel_fail_at = -1;
    if (file[4] != 2 && file != staged) {
        copy_bytes(staged, file, (uint32_t)g_kernelSize);
    }
    return load_elf();
}

/* ---- ELF64 ------------------------------------------------------------ */

static Elf64_Header *h64(void) { return (Elf64_Header *)file; }
static Elf64_Phdr *ph64(int i) { return (Elf64_Phdr *)(file + 64 + i * sizeof(Elf64_Phdr)); }

/* A good 64-bit kernel, as the real one is linked: two segments, code
 * and data, at 1 MiB physical, in the kernel window; data has 0x1000
 * bytes of .bss. The file is 0x3000 bytes. */
#define FILE64 0x3000u

static void good64(void)
{
    if (!file) {
        file = allocate(8u << 20);
    }
    memory_map_default();
    set_bytes(file, 0, FILE64);
    Elf64_Header *h = h64();
    h->e_ident[0] = ELFMAG0; h->e_ident[1] = ELFMAG1; h->e_ident[2] = ELFMAG2; h->e_ident[3] = ELFMAG3;
    h->e_ident[4] = 2;  /* 64-bit */
    h->e_ident[5] = 1;  /* little-endian */
    h->e_ident[6] = 1;
    h->e_type = ET_EXEC;
    h->e_machine = EM_X86_64;
    h->e_version = 1;
    h->e_entry = WINDOW + 0x100010;
    h->e_phoff = 64;
    h->e_ehsize = 64;
    h->e_phentsize = sizeof(Elf64_Phdr);
    h->e_phnum = 2;

    Elf64_Phdr *code = ph64(0);
    code->p_type = PT_LOAD;
    code->p_offset = 0x1000;
    code->p_vaddr = WINDOW + 0x100000;
    code->p_paddr = 0x100000;
    code->p_filesz = code->p_memsz = 0x1000;

    Elf64_Phdr *data = ph64(1);
    data->p_type = PT_LOAD;
    data->p_offset = 0x2000;
    data->p_vaddr = WINDOW + 0x101000;
    data->p_paddr = 0x101000;
    data->p_filesz = 0x1000;
    data->p_memsz = 0x2000;

    for (uint32_t i = 0x1000; i < FILE64; i++) {
        file[i] = (uint8_t)(i * 3 + 1);
    }
    g_kernelSize = FILE64;
    /* Where it goes: garbage first, so that a .bss not zeroed shows. */
    set_bytes((void *)0x100000, 0xAA, 0x4000);
    printed_clear();
}

static void loads_a_good_64_bit_kernel(void)
{
    good64();
    CHECK(load() == 1, "a good 64-bit kernel loads");
    CHECK(g_kernelIs64 == 1, "and is taken for 64-bit");
    CHECK(same_bytes((void *)0x100000, file + 0x1000, 0x2000), "both segments' bytes at their physical places");
    int zero = 1;
    for (uint8_t *p = (uint8_t *)0x102000; p < (uint8_t *)0x103000; p++) {
        zero &= *p == 0;
    }
    CHECK(zero, "the .bss after the data is zeroed");
    CHECK(*(uint8_t *)0x103000 == 0xAA, "and nothing past it is touched");
    CHECK(g_kernelAddress == 0x100000 && g_kernelEnd == 0x103000, "the physical span, .bss included");
    CHECK(g_kernelVirtual == WINDOW + 0x100000, "the lowest segment's virtual address");
    CHECK(g_kernelEntry64 == WINDOW + 0x100010, "the entry, virtual");
}

static void loads_an_identity_mapped_64_bit_kernel(void)
{
    good64();
    ph64(0)->p_vaddr = 0x100000;
    ph64(1)->p_vaddr = 0x101000;
    h64()->e_entry = 0x100010;
    CHECK(load() == 1, "linked at its physical address is allowed too");
}

/* Each way a 64-bit kernel is refused: what is changed from good64, and
 * the message that must say why. */
static void refuses64(const char *what, const char *message)
{
    int loaded = load();
    if (loaded != 0) {
        out("  (refusal expected: ");
        out(what);
        out(")\n");
    }
    CHECK(loaded == 0, what);
    if (!printed_contains(message)) {
        out("  (expected message: ");
        out(message);
        out(")\n");
    }
    CHECK(printed_contains(message), what);
}

static void refuses_bad_64_bit_kernels(void)
{
    good64(); file[1] = 'X';
    refuses64("not an ELF file", "Not an ELF file");
    good64(); h64()->e_ident[5] = 2;
    refuses64("big-endian", "Not little-endian");
    good64(); h64()->e_machine = 183;
    refuses64("an AArch64 kernel", "Not an x86_64 ELF");
    good64(); h64()->e_type = ET_DYN;
    refuses64("position-independent (ET_DYN)", "Not an executable ELF");
    good64(); h64()->e_phnum = 0;
    refuses64("no program headers", "Bad program header table");
    good64(); h64()->e_phentsize = 32;
    refuses64("program headers smaller than Elf64_Phdr", "Bad program header table");
    good64(); h64()->e_phoff = FILE64 - 64;
    refuses64("program headers past the file's end", "Program headers run past end of file");
    good64(); ph64(0)->p_paddr = 0x80000; ph64(0)->p_vaddr = WINDOW + 0x80000;
    refuses64("a segment below 1 MiB, on the loader", "outside 1 MiB .. 1 GiB");
    good64(); ph64(1)->p_memsz = 0x40000000;
    refuses64("a segment reaching past 1 GiB, the kernel window's end", "outside 1 MiB .. 1 GiB");
    good64(); ph64(1)->p_paddr = 0xFFFFFFFFFFFFF000ULL;
    refuses64("a segment whose end wraps past 2^64", "outside 1 MiB .. 1 GiB");
    good64(); ph64(0)->p_vaddr = 0xFFFF800000100000ULL;
    refuses64("a segment neither identity- nor window-mapped", "neither identity- nor window-mapped");
    good64(); ph64(1)->p_filesz = 0x3000;
    refuses64("p_filesz > p_memsz", "Segment runs past end of file");
    good64(); ph64(1)->p_offset = FILE64 - 0x800;
    refuses64("a segment's bytes past the file's end", "Segment runs past end of file");
    good64(); ph64(0)->p_type = PT_NOTE; ph64(1)->p_type = PT_NOTE;
    refuses64("no PT_LOAD at all", "No loadable segments");
    good64(); h64()->e_entry = WINDOW + 0x200000;
    refuses64("an entry outside every segment", "e_entry is outside");
    /* A program header table at an offset so large that offset + its
     * size wraps past 2^64 - small again - and passes the check against
     * the file's size. */
    good64(); h64()->e_phoff = 0xFFFFFFFFFFFFFFC0ULL;
    refuses64("e_phoff + the table wrapping past 2^64", "Program headers run past end of file");
    /* The same for a segment's bytes. */
    good64(); ph64(0)->p_offset = 0xFFFFFFFFFFFFF800ULL;
    refuses64("p_offset + p_filesz wrapping past 2^64", "Segment runs past end of file");
}

/* ---- ELF32: the 32-bit kernel, still loaded as before ------------------ */

static Elf32_Header *h32(void) { return (Elf32_Header *)staged; }
static Elf32_Phdr *ph32(int i) { return (Elf32_Phdr *)(staged + 52 + i * sizeof(Elf32_Phdr)); }

#define FILE32 0x2000u

static void good32(void)
{
    file = staged; /* a 32-bit kernel is staged: made where it is read */
    memory_map_default();
    set_bytes(staged, 0, FILE32);
    Elf32_Header *h = h32();
    h->e_ident[0] = ELFMAG0; h->e_ident[1] = ELFMAG1; h->e_ident[2] = ELFMAG2; h->e_ident[3] = ELFMAG3;
    h->e_ident[4] = 1;
    h->e_ident[5] = 1;
    h->e_ident[6] = 1;
    h->e_type = ET_EXEC;
    h->e_machine = EM_386;
    h->e_version = 1;
    h->e_entry = 0x101000;
    h->e_phoff = 52;
    h->e_ehsize = 52;
    h->e_phentsize = sizeof(Elf32_Phdr);
    h->e_phnum = 1;
    Elf32_Phdr *p = ph32(0);
    p->p_type = PT_LOAD;
    p->p_offset = 0x1000;
    p->p_vaddr = p->p_paddr = 0x100000;
    p->p_filesz = 0x1000;
    p->p_memsz = 0x1800;
    for (uint32_t i = 0x1000; i < FILE32; i++) {
        staged[i] = (uint8_t)(i * 5 + 3);
    }
    g_kernelSize = FILE32;
    set_bytes((void *)0x100000, 0xAA, 0x2000);
    printed_clear();
}

static void loads_and_refuses_32_bit_kernels(void)
{
    good32();
    CHECK(load() == 1 && g_kernelIs64 == 0, "a good 32-bit kernel loads, as 32-bit");
    CHECK(same_bytes((void *)0x100000, staged + 0x1000, 0x1000), "its bytes");
    CHECK(*(uint8_t *)0x101000 == 0 && *(uint8_t *)0x1017FF == 0 && *(uint8_t *)0x101800 == 0xAA, "its .bss zeroed, no more");
    CHECK(g_kernelAddress == 0x100000 && g_kernelEnd == 0x101800 && g_kernelEntry == 0x101000, "its span and entry");

    good32(); h32()->e_machine = EM_X86_64;
    refuses64("a 32-bit file for x86_64", "Not an i386 ELF");
    good32(); ph32(0)->p_paddr = 0x2FF000;
    refuses64("a segment over the staging buffer", "overlaps the ELF staging buffer");
    good32(); ph32(0)->p_paddr = 0xFFFFF000;
    refuses64("a segment whose end wraps past 4 GiB", "Segment address overflows");
    good32(); h32()->e_entry = 0x200000;
    refuses64("an entry outside the image", "e_entry is outside");
    /* Wrapping past 4 GiB, as for ELF64 above. */
    good32(); h32()->e_phoff = 0xFFFFFFF0u;
    refuses64("e_phoff + the table wrapping past 4 GiB", "Program headers run past end of file");
    good32(); ph32(0)->p_offset = 0xFFFFF800u;
    refuses64("p_offset + p_filesz wrapping past 4 GiB", "Segment runs past end of file");
}

/* ---- any size, anywhere usable (K130) ------------------------------------------ */

/* Past the old wall: a 4 MiB segment at 8 MiB physical, from a file of
 * 4 MiB and more - which could be neither staged at 3 MiB nor loaded
 * below it before. */
static void loads_a_kernel_past_the_old_3_mib_wall(void)
{
    good64();
    const uint32_t size = 4u << 20;
    Elf64_Phdr *big = ph64(1);
    big->p_offset = 0x2000;
    big->p_paddr = 8u << 20;
    big->p_vaddr = WINDOW + (8u << 20);
    big->p_filesz = size;
    big->p_memsz = size + 0x1000;
    for (uint32_t i = 0; i < size; i++) {
        file[0x2000 + i] = (uint8_t)(i * 7 + (i >> 12));
    }
    g_kernelSize = (int)(0x2000 + size);
    set_bytes((void *)(8u << 20), 0xAA, size + 0x2000);
    kernel_reads = 0;
    CHECK(load() == 1, "a kernel with a 4 MiB segment at 8 MiB loads");
    CHECK(same_bytes((void *)(8u << 20), file + 0x2000, size), "every byte of it, read from the file to its place");
    CHECK(*(uint8_t *)((8u << 20) + size) == 0 && *(uint8_t *)((8u << 20) + size + 0xFFF) == 0
              && *(uint8_t *)((8u << 20) + size + 0x1000) == 0xAA,
          "its .bss zeroed, and nothing past it");
    CHECK(g_kernelEnd == (8u << 20) + size + 0x1000, "the kernel's end, for the memory map and the ramdisk");
    CHECK(kernel_reads <= 2 + 2, "a read per program header and per segment - no staging copy");
}

static void refuses_a_segment_outside_usable_memory(void)
{
    good64();
    /* A hole at 1 MiB + 4 KiB: the data segment lands on it. */
    const MemoryRegion holed[] = {
        { 0, 0x9FC00, 1 },
        { 0x100000, 0x1000, 1 },
        { 0x101000, 0x1000, 2 },
        { 0x102000, 0x1FEFE000, 1 },
    };
    memory_map(holed, 4);
    printed_clear();
    CHECK(load() == 0 && printed_contains("not all in usable memory"), "a segment over reserved memory is refused");

    good64();
    const MemoryRegion small[] = { { 0, 0x9FC00, 1 }, { 0x100000, 0x1800, 1 } };
    memory_map(small, 2);
    printed_clear();
    CHECK(load() == 0 && printed_contains("not all in usable memory"), "one past the end of memory too");
}

static void a_read_error_fails_the_load(void)
{
    good64();
    copy_bytes(g_kernelHead, file, 64);
    kernel_file = file;
    kernel_file_size = FILE64;
    kernel_fail_at = 0x2100; /* in the data segment */
    printed_clear();
    CHECK(load_elf() == 0 && printed_contains("could not be read"), "a segment the disk cannot give is a refusal");
    kernel_fail_at = 70; /* in the first program header */
    printed_clear();
    CHECK(load_elf() == 0 && printed_contains("program header could not be read"), "so is a program header");
    kernel_fail_at = -1;
}

static void usable_memory_follows_the_map(void)
{
    const MemoryRegion map[] = {
        { 0x100000, 0x100000, 1 },  /* 1-2 MiB */
        { 0x200000, 0x100000, 1 },  /* 2-3 MiB, right after: one stretch */
        { 0x400000, 0x100000, 1 },  /* 4-5 MiB, after a gap */
        { 0x480000, 0x1000, 2 },    /* inside it, something else */
    };
    memory_map(map, 4);
    CHECK(usable_memory(0x100000, 0x300000), "two usable regions next to each other count as one");
    CHECK(!usable_memory(0x2FF000, 0x401000), "not across a gap");
    CHECK(usable_memory(0x400000, 0x480000), "up to a reserved region inside a usable one");
    CHECK(!usable_memory(0x47F000, 0x481000), "not over it");
    CHECK(!usable_memory(0x500000, 0x501000), "not past the end");
    CHECK(usable_memory(0x200000, 0x200000), "nothing at all is fine");
    memory_map_default();
}

/* ---- the real kernel ------------------------------------------------------ */

static void the_build_s_kernel(const char *path)
{
    if (!path) {
        out("elf: no real kernel given, its checks skipped\n");
        return;
    }
    uint32_t size = 0;
    uint8_t *real = read_file(path, &size);
    CHECK(real != 0, "the build's kernel can be read");
    if (!real) {
        return;
    }
    memory_map_default();
    uint8_t *kept = file;
    file = real;
    g_kernelSize = (int)size;
    printed_clear();
    CHECK(load() == 1 && g_kernelIs64, "the build's own kernel loads, as 64-bit, read from its file in place");
    CHECK(g_kernelEntry64 == ((Elf64_Header *)real)->e_entry, "its entry is the file's");
    CHECK(g_kernelEntry64 >= WINDOW, "in the kernel window: a higher-half kernel");
    CHECK(g_kernelAddress >= 0x100000 && g_kernelEnd <= 0x40000000u, "between 1 MiB and 1 GiB");
    file = kept;
}

void elf_tests(const char *real_kernel)
{
    loads_a_good_64_bit_kernel();
    loads_an_identity_mapped_64_bit_kernel();
    refuses_bad_64_bit_kernels();
    loads_and_refuses_32_bit_kernels();
    loads_a_kernel_past_the_old_3_mib_wall();
    refuses_a_segment_outside_usable_memory();
    a_read_error_fails_the_load();
    usable_memory_follows_the_map();
    the_build_s_kernel(real_kernel);
}
