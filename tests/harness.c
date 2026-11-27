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
int32_t  disk_fail_at = -1;

void disk_use(uint8_t *image, uint32_t sectors)
{
    disk = image;
    disk_sectors = sectors;
    disk_reads = 0;
    disk_fail_at = -1;
}

/* bios_disk.h: one 2048-byte sector, 0 on success. Past the image's end
 * is a read error, as on a real disc. */
int read_sector(unsigned int lba, unsigned char *buffer)
{
    disk_reads++;
    if (lba >= disk_sectors || (int32_t)lba == disk_fail_at) {
        return 1;
    }
    copy_bytes(buffer, disk + lba * SECTOR, SECTOR);
    return 0;
}

/* elf.c reads it; vfs.c, not built here, sets it. */
int g_kernelSize;
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
        out("harness: could not map stage 2's addresses 0x80000-0x1000000\n");
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
