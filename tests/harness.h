/* harness.h - host tests for stage 2's C code (K129).
 *
 * Stage 2 runs in 32-bit protected mode, with paging off, at fixed
 * physical addresses: the ISO reader's buffers at 0x80000 and 0x90000,
 * the kernel file staged at 0x300000, the kernel's segments copied to
 * 1-3 MiB. So the tests are 32-bit programs too, built with stage 2's own
 * compiler and flags, freestanding like it - no C library on either side
 * - and they ask Linux for memory at exactly those addresses. The code
 * under test is the code that boots, compiled from the same files,
 * unchanged.
 *
 * Linux puts nothing that low in a 32-bit process (its own code starts
 * at 0x08048000), and refuses only the first 64 KiB (vm.mmap_min_addr):
 * the BIOS data area at 0x400 and the RSDP search at 0xE0000 are the
 * only addresses stage 2 reads that a test cannot give it - so FindRsdp
 * is the one function here not run whole; its checksum test is.
 *
 * What touches hardware is replaced: bios_read - stage2.asm's trip to
 * real mode for int 13h - reads from a disk image in memory into the
 * BIOS buffer at 0x60000, as the BIOS would (K131: so bios_disk.c's own
 * reading is tested too); and boot_print keeps what is printed, so that
 * a test can check which message a refusal printed. */

#ifndef HARNESS_H
#define HARNESS_H

#include <stdint.h>
#include <stddef.h>

/* The low memory every test may use: everything stage 2's C code touches
 * at a fixed address, and room for the kernel file. */
#define LOW_MEMORY_START 0x60000u     /* the BIOS buffer */
#define LOW_MEMORY_END   0x1000000u     /* 16 MiB */

/* ---- output ---------------------------------------------------------- */

void out(const char *s);
void out_dec(uint32_t value);
void out_hex(uint32_t value);

/* ---- checks ---------------------------------------------------------- */

extern int checks_run;
extern int checks_failed;

void check_failed(const char *file, int line, const char *what, const char *why);

/* CHECK(condition, "why it matters"): counted; a failure says where,
 * what, and why, and the run goes on - one bug, every symptom. */
#define CHECK(cond, why)                                         \
    do {                                                         \
        checks_run++;                                            \
        if (!(cond)) {                                           \
            check_failed(__FILE__, __LINE__, #cond, why);        \
        }                                                        \
    } while (0)

/* ---- what stage 2 printed -------------------------------------------- */

void printed_clear(void);
int printed_contains(const char *text);

/* ---- the disk bios_read reads ------------------------------------------ */

#define SECTOR 2048u

extern uint8_t *disk;            /* the image, `disk_sectors` ISO sectors */
extern uint32_t disk_sectors;
extern uint32_t disk_reads;      /* bios_read calls since disk_use       */
extern uint32_t disk_most;       /* the most ISO sectors one call read   */
extern int      disk_broke_rule; /* a call the BIOS would refuse: more
                                  * than 127 sectors, past the buffer's
                                  * 128 KiB, or across a 64 KiB block    */
extern int32_t  disk_fail_at;    /* this ISO sector fails to read; -1: none */
extern int32_t  disk_fail_times; /* how often it fails; -1: every time   */
/* The disk's own sector size, as stage2.asm asks the BIOS: 2048 for a
 * CD, 512 for a stick (bios_disk.c reads this). */
extern unsigned short wDiskSectorSize;

void disk_use(uint8_t *image, uint32_t sectors);

/* ---- memory and files ------------------------------------------------ */

/* Fresh zeroed memory anywhere (mmap): for disk images and the like. */
void *allocate(uint32_t bytes);
/* A whole file into fresh memory; its size in *size. 0 if unreadable. */
void *read_file(const char *path, uint32_t *size);

void set_bytes(void *dest, int value, uint32_t count);
void copy_bytes(void *dest, const void *src, uint32_t count);
int same_bytes(const void *a, const void *b, uint32_t count);
uint32_t text_length(const char *s);

/* What vfs.c, not built here, would have set: the staged kernel's size
 * (elf.c reads it), the ramdisk's place (longmode.c reads it). */
extern int g_kernelSize;
extern unsigned int g_ramdiskAddress;
extern int g_ramdiskSize;

/* ---- the suites ------------------------------------------------------ */

void iso_tests(const char *real_iso);
void elf_tests(const char *real_kernel);
void longmode_tests(void);

#endif
