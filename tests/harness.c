/* harness.c - the start, the system calls and the checks of stage 2's
 * host tests (K129). See harness.h for why they look like this. */

#include "harness.h"

/* ---- Linux, 32-bit, by hand ---------------------------------------- */

#define SYS_EXIT   1
#define SYS_READ   3
#define SYS_WRITE  4
#define SYS_OPEN   5
#define SYS_CLOSE  6
#define SYS_MMAP2  192

#define PROT_READ_WRITE  3
#define MAP_PRIVATE      0x02
#define MAP_ANONYMOUS    0x20
#define MAP_FIXED_NOREPLACE 0x100000

/* int 0x80 with up to six arguments. The sixth goes in EBP, which the
 * compiler may be using as the frame pointer: saved and restored here. */
static int32_t sys(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    int32_t result;
    __asm__ volatile ("push %%ebp\n\t"
                      "mov %7, %%ebp\n\t"
                      "int $0x80\n\t"
                      "pop %%ebp"
                      : "=a"(result)
                      : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e), "g"(f)
                      : "memory");
    return result;
}

static void quit(int code)
{
    for (;;) {
        sys(SYS_EXIT, code, 0, 0, 0, 0, 0);
    }
}

/* ---- output ---------------------------------------------------------- */

uint32_t text_length(const char *s)
{
    uint32_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

void out(const char *s)
{
    sys(SYS_WRITE, 1, (int32_t)s, (int32_t)text_length(s), 0, 0, 0);
}

void out_dec(uint32_t value)
{
    char digits[11];
    int at = 10;
    digits[at] = 0;
    do {
        digits[--at] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    out(digits + at);
}

void out_hex(uint32_t value)
{
    char digits[11] = "0x00000000";
    for (int i = 9; i >= 2; i--, value >>= 4) {
        digits[i] = "0123456789abcdef"[value & 15];
    }
    out(digits);
}

/* ---- checks ---------------------------------------------------------- */

int checks_run;
int checks_failed;

void check_failed(const char *file, int line, const char *what, const char *why)
{
    checks_failed++;
    out("FAILED ");
    out(file);
    out(":");
    out_dec((uint32_t)line);
    out(": ");
    out(what);
    out(" - ");
    out(why);
    out("\n");
}

/* ---- bytes ----------------------------------------------------------- */

void set_bytes(void *dest, int value, uint32_t count)
{
    uint8_t *d = dest;
    while (count--) {
        *d++ = (uint8_t)value;
    }
}

void copy_bytes(void *dest, const void *src, uint32_t count)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (count--) {
        *d++ = *s++;
    }
}

int same_bytes(const void *a, const void *b, uint32_t count)
{
    const uint8_t *x = a, *y = b;
    while (count--) {
        if (*x++ != *y++) {
            return 0;
        }
    }
    return 1;
}

/* ---- memory and files ------------------------------------------------ */

void *allocate(uint32_t bytes)
{
    int32_t at = sys(SYS_MMAP2, 0, (int32_t)bytes, PROT_READ_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((uint32_t)at >= 0xFFFFF000u) {
        out("harness: out of memory\n");
        quit(2);
    }
    return (void *)at;
}

void *read_file(const char *path, uint32_t *size)
{
    int32_t fd = sys(SYS_OPEN, (int32_t)path, 0, 0, 0, 0, 0);
    if (fd < 0) {
        return 0;
    }
    /* Grown by doubling: the file's size is not asked for, only read. */
    uint32_t capacity = 1u << 20, used = 0;
    uint8_t *data = allocate(capacity);
    for (;;) {
        if (used == capacity) {
            uint8_t *bigger = allocate(capacity * 2);
            copy_bytes(bigger, data, used);
            data = bigger; /* the old block is left: these runs are short */
            capacity *= 2;
        }
        int32_t got = sys(SYS_READ, fd, (int32_t)(data + used), (int32_t)(capacity - used), 0, 0, 0);
        if (got < 0) {
            sys(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
            return 0;
        }
        if (got == 0) {
            break;
        }
        used += (uint32_t)got;
    }
    sys(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    *size = used;
    return data;
}

/* ---- stage 2's printing, kept ----------------------------------------- */

static char printed[16384];
static uint32_t printed_length;

void printed_clear(void)
{
    printed_length = 0;
    printed[0] = 0;
}

static void keep(const char *s)
{
    while (*s && printed_length < sizeof printed - 1) {
        printed[printed_length++] = *s++;
    }
    printed[printed_length] = 0;
}

int printed_contains(const char *text)
{
    uint32_t n = text_length(text);
    for (uint32_t at = 0; at + n <= printed_length; at++) {
        if (same_bytes(printed + at, text, n)) {
            return 1;
        }
    }
    return 0;
}

/* print.h's signatures: stage 2 passes string literals as unsigned char*. */
void boot_print(unsigned char *str)
{
    keep((const char *)str);
}

void boot_print_hex(unsigned int value)
{
    char digits[11] = "0x00000000";
    for (int i = 9; i >= 2; i--, value >>= 4) {
        digits[i] = "0123456789ABCDEF"[value & 15];
    }
    keep(digits);
}

/* ---- the disk ---------------------------------------------------------- */

uint8_t *disk;
uint32_t disk_sectors;
uint32_t disk_reads;
uint32_t disk_most;
int      disk_broke_rule;
int32_t  disk_fail_at = -1;
int32_t  disk_fail_times = -1;
unsigned short wDiskSectorSize = 2048;

void disk_use(uint8_t *image, uint32_t sectors)
{
    disk = image;
    disk_sectors = sectors;
    disk_reads = 0;
    disk_most = 0;
    disk_broke_rule = 0;
    disk_fail_at = -1;
    disk_fail_times = -1;
    wDiskSectorSize = 2048;
}

#define BIOS_BUFFER 0x60000u

/* stage2.asm's bios_read: `count` of the disk's own sectors (2048 or 512
 * bytes) from `lba`, into the BIOS buffer; 0, or a BIOS error code. Past
 * the image's end is an error, as on a real disc; so is a sector told to
 * fail. A call a real BIOS would refuse is done anyway, and noted. */
unsigned int bios_read(unsigned int lba, unsigned int count)
{
    uint32_t size = wDiskSectorSize;
    uint32_t start = lba * size, bytes = count * size;
    disk_reads++;
    if (count == 0 || count > 127 || bytes > 0x20000u
        || (BIOS_BUFFER >> 16) != ((BIOS_BUFFER + bytes - 1) >> 16)) {
        disk_broke_rule = 1;
    }
    uint32_t first = start / SECTOR, last = (start + bytes - 1) / SECTOR;
    if (last >= disk_sectors) {
        return 0x04; /* sector not found */
    }
    if (disk_fail_at >= 0 && (uint32_t)disk_fail_at >= first && (uint32_t)disk_fail_at <= last
        && disk_fail_times != 0) {
        if (disk_fail_times > 0) {
            disk_fail_times--;
        }
        return 0x04;
    }
    if (last - first + 1 > disk_most) {
        disk_most = last - first + 1;
    }
    copy_bytes((void *)BIOS_BUFFER, disk + start, bytes);
    return 0;
}

/* elf.c reads it; vfs.c, not built here, sets it. */
int g_kernelSize;

/* ---- the kernel's file, and the memory map (K130) ------------------------- */

uint8_t *kernel_file;
uint32_t kernel_file_size;
int32_t  kernel_fail_at = -1;
uint32_t kernel_reads;

/* vfs.c's: bytes of the kernel's file into place. As iso9660_read: cut at
 * the file's end, 0 from past it, -1 on a read error. */
int kernel_read(void *dest, unsigned int size, unsigned int offset)
{
    kernel_reads++;
    if (!kernel_file) {
        return -1;
    }
    if (offset >= kernel_file_size) {
        return 0;
    }
    if (size > kernel_file_size - offset) {
        size = kernel_file_size - offset;
    }
    if (kernel_fail_at >= 0 && (uint32_t)kernel_fail_at >= offset && (uint32_t)kernel_fail_at < offset + size) {
        return -1;
    }
    copy_bytes(dest, kernel_file + offset, size);
    return (int)size;
}

/* stage2.asm's MultiBoot structure, as far as elf.c reads it: the count
 * of E820 entries at 11 * 4, their address at 12 * 4. Each entry as
 * LoadMemoryMap stores it: 24 bytes, padded to 32. */
unsigned char BootHeader[0x60] __attribute__((aligned(32)));
static struct {
    uint64_t base, length;
    uint32_t type, acpi, pad[2];
} e820[32];

void memory_map(const MemoryRegion *regions, uint32_t count)
{
    set_bytes(e820, 0, sizeof e820);
    for (uint32_t i = 0; i < count && i < 32; i++) {
        e820[i].base = regions[i].base;
        e820[i].length = regions[i].length;
        e820[i].type = regions[i].type;
    }
    *(uint32_t *)(BootHeader + 11 * 4) = count;
    *(uint32_t *)(BootHeader + 12 * 4) = (uint32_t)e820;
}

void memory_map_default(void)
{
    static const MemoryRegion pc[] = {
        { 0, 0x9FC00, 1 },
        { 0x9FC00, 0x400, 2 },
        { 0xF0000, 0x10000, 2 },
        { 0x100000, 0x1FF00000, 1 },
        { 0xFFFC0000, 0x40000, 2 },
    };
    memory_map(pc, sizeof pc / sizeof pc[0]);
}
/* longmode.c reads these; vfs.c sets them. */
unsigned int g_ramdiskAddress;
int g_ramdiskSize;

/* ---- the start ------------------------------------------------------- */

int run(int argc, char **argv)
{
    /* Stage 2's fixed addresses, ours: refused if anything is there. */
    int32_t at = sys(SYS_MMAP2, LOW_MEMORY_START, LOW_MEMORY_END - LOW_MEMORY_START, PROT_READ_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (at != (int32_t)LOW_MEMORY_START) {
        out("harness: could not map stage 2's addresses 0x60000-0x1000000\n");
        return 2;
    }

    /* Optional real files: an ISO the build made, and the kernel on it. */
    const char *real_iso = argc > 1 ? argv[1] : 0;
    const char *real_kernel = argc > 2 ? argv[2] : 0;

    iso_tests(real_iso);
    elf_tests(real_kernel);
    longmode_tests();

    out("stage2 host tests: ");
    out_dec((uint32_t)checks_run);
    out(" checks, ");
    out_dec((uint32_t)checks_failed);
    out(" failed\n");
    return checks_failed ? 1 : 0;
}

void start(uint32_t *stack)
{
    int argc = (int)stack[0];
    char **argv = (char **)(stack + 1);
    quit(run(argc, argv));
}

/* The kernel enters at _start with argc at the stack pointer. The stack
 * is aligned for the C code, which the System V i386 ABI asks 16 for. */
__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    mov %esp, %eax\n"
        "    and $-16, %esp\n"
        "    sub $12, %esp\n"
        "    push %eax\n"
        "    call start\n"
        "    hlt\n");
