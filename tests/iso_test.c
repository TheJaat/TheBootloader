/* iso_test.c - the ISO 9660 reader (ISO9660FS.c), on images made here
 * sector by sector - good ones, and the damaged ones a disc can hold -
 * and on a real ISO the build made, if one is given. */

#include "harness.h"
#include <ISO9660.h>
#include <ISO9660FS.h>
#include <vfs.h>

extern iso9660_fs_t iso_fs;

/* ---- making images ------------------------------------------------------ */

static uint8_t *image;
static uint32_t image_sectors;

static void image_new(uint32_t sectors)
{
    image = allocate(sectors * SECTOR);
    image_sectors = sectors;
    disk_use(image, sectors);
    printed_clear();
}

static uint8_t *sector_at(uint32_t lba)
{
    return image + lba * SECTOR;
}

static void both_endian32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
    at[4] = (uint8_t)(value >> 24);
    at[5] = (uint8_t)(value >> 16);
    at[6] = (uint8_t)(value >> 8);
    at[7] = (uint8_t)value;
}

/* One directory record at `at`: the standard's layout, by offset - not
 * the header's struct, so that a mistake in the struct shows. Returns
 * its length (33 + name, padded to even). */
static uint32_t record(uint8_t *at, const char *name, uint32_t name_len, uint32_t extent, uint32_t size, int directory)
{
    uint32_t length = 33 + name_len;
    length += length & 1;
    set_bytes(at, 0, length);
    at[0] = (uint8_t)length;
    both_endian32(at + 2, extent);
    both_endian32(at + 10, size);
    at[25] = directory ? FLAG_DIRECTORY : 0;
    at[28] = 1; /* volume sequence 1, both-endian */
    at[29] = 1;
    at[32] = (uint8_t)name_len;
    copy_bytes(at + 33, name, name_len);
    return length;
}

/* A directory's first records, "." and "..": names 0x00 and 0x01. */
static uint32_t dots(uint8_t *at, uint32_t self, uint32_t parent, uint32_t size)
{
    uint32_t used = record(at, "\0", 1, self, size, 1);
    return used + record(at + used, "\1", 1, parent, size, 1);
}

/* The primary volume descriptor at sector 16, and the terminator at 17. */
static void volume(uint32_t root_extent, uint32_t root_size)
{
    uint8_t *pvd = sector_at(16);
    pvd[0] = 1;
    copy_bytes(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    record(pvd + 156, "\0", 1, root_extent, root_size, 1);
    uint8_t *end = sector_at(17);
    end[0] = 255;
    copy_bytes(end + 1, "CD001", 5);
    end[6] = 1;
}

/* A file's bytes: each byte says where it is, so a read from the wrong
 * place shows. */
static uint8_t pattern(uint32_t extent, uint32_t offset)
{
    return (uint8_t)(offset * 7 + extent * 13 + (offset >> 8));
}

static void file_bytes(uint32_t extent, uint32_t size)
{
    uint8_t *at = sector_at(extent);
    for (uint32_t i = 0; i < size; i++) {
        at[i] = pattern(extent, i);
    }
}

/* The test volume:
 *
 *   /                  sector 20
 *   /KERNEL.ELF;1      sectors 30-32, 5000 bytes
 *   /BOOT/             sector 21
 *   /BOOT/GRUB/        sector 22
 *   /BOOT/GRUB/X.CFG;1 sector 33, 10 bytes
 *   /BIG/              sectors 23-24: two sectors of records
 *   /BIG/LAST.TXT;1    in the second, sector 34, 3 bytes
 */
#define ROOT 20
#define KERNEL_SIZE 5000

static void standard_image(void)
{
    image_new(40);
    volume(ROOT, SECTOR);

    uint8_t *root = sector_at(ROOT);
    uint32_t at = dots(root, ROOT, ROOT, SECTOR);
    at += record(root + at, "BIG", 3, 23, 2 * SECTOR, 1);
    at += record(root + at, "BOOT", 4, 21, SECTOR, 1);
    at += record(root + at, "KERNEL.ELF;1", 12, 30, KERNEL_SIZE, 0);

    uint8_t *boot = sector_at(21);
    at = dots(boot, 21, ROOT, SECTOR);
    record(boot + at, "GRUB", 4, 22, SECTOR, 1);

    uint8_t *grub = sector_at(22);
    at = dots(grub, 22, 21, SECTOR);
    record(grub + at, "X.CFG;1", 7, 33, 10, 0);

    /* /BIG: the first sector full of names, LAST.TXT in the second. */
    uint8_t *big = sector_at(23);
    at = dots(big, 23, ROOT, 2 * SECTOR);
    char name[] = "FILL00.TXT;1";
    for (int i = 0; at + 46 <= SECTOR; i++) {
        name[4] = (char)('0' + i / 10);
        name[5] = (char)('0' + i % 10);
        at += record(big + at, name, 12, 35, 1, 0);
    }
    record(sector_at(24), "LAST.TXT;1", 10, 34, 3, 0);

    file_bytes(30, KERNEL_SIZE);
    file_bytes(33, 10);
    file_bytes(34, 3);
}

/* ---- the tests -------------------------------------------------------- */

static void layout_matches_the_standard(void)
{
    CHECK(__builtin_offsetof(iso_9660_volume_descriptor_t, root) == 156, "the root record is at byte 156 of the PVD");
    CHECK(__builtin_offsetof(iso_9660_directory_entry_t, extent_start_LSB) == 2, "a record's extent is at byte 2");
    CHECK(__builtin_offsetof(iso_9660_directory_entry_t, extent_length_LSB) == 10, "a record's size is at byte 10");
    CHECK(__builtin_offsetof(iso_9660_directory_entry_t, flags) == 25, "a record's flags are at byte 25");
    CHECK(__builtin_offsetof(iso_9660_directory_entry_t, name_len) == 32, "a record's name length is at byte 32");
    CHECK(__builtin_offsetof(iso_9660_directory_entry_t, name) == 33, "a record's name is at byte 33");
}

static void mounts_a_good_volume(void)
{
    standard_image();
    CHECK(iso9660_mount() == 0, "a good volume mounts");
    CHECK(iso_fs.root_dir_sector == ROOT, "the root directory's sector, from the PVD");
    CHECK(iso_fs.root_dir_size == SECTOR, "the root directory's size, from the PVD");
}

static void refuses_what_is_not_iso9660(void)
{
    standard_image();
    copy_bytes(sector_at(16) + 1, "CD002", 5);
    CHECK(iso9660_mount() == -1, "no CD001: not mounted");
    CHECK(printed_contains("Not a valid ISO9660 FS"), "and it says why");

    standard_image();
    disk_fail_at = 16;
    CHECK(iso9660_mount() == -1, "the PVD unreadable: not mounted");
    CHECK(printed_contains("Unable to read"), "and it says why");
}

static void opens_the_root_and_files_in_it(void)
{
    standard_image();
    iso9660_mount();

    vfs_node_t *root = iso9660_open("/");
    CHECK(root && root->is_directory && root->sector == ROOT, "\"/\" is the root directory");

    vfs_node_t *kernel = iso9660_open("/KERNEL.ELF");
    CHECK(kernel != 0, "a file in the root is found, without its \";1\"");
    if (kernel) {
        CHECK(kernel->sector == 30 && kernel->size == KERNEL_SIZE && !kernel->is_directory, "its place, size and kind");
    }
    CHECK(iso9660_open("KERNEL.ELF") != 0, "a path without the leading slash");
    CHECK(iso9660_open("///KERNEL.ELF") != 0, "several leading slashes");
}

static void walks_nested_directories(void)
{
    standard_image();
    iso9660_mount();

    vfs_node_t *cfg = iso9660_open("/BOOT/GRUB/X.CFG");
    CHECK(cfg && cfg->sector == 33 && cfg->size == 10, "a file two directories down");
    CHECK(iso9660_open("/BOOT//GRUB///X.CFG") != 0, "repeated slashes between names");
    vfs_node_t *grub = iso9660_open("/BOOT/GRUB/");
    CHECK(grub && grub->is_directory && grub->sector == 22, "a directory, with a trailing slash");
}

static void finds_a_name_in_a_directory_s_second_sector(void)
{
    standard_image();
    iso9660_mount();
    vfs_node_t *last = iso9660_open("/BIG/LAST.TXT");
    CHECK(last && last->sector == 34 && last->size == 3, "records continue in the directory's next sector");
}

static void says_what_is_missing(void)
{
    standard_image();
    iso9660_mount();
    CHECK(iso9660_open("/NOPE.TXT") == 0, "a name that is not there");
    CHECK(printed_contains("Cannot find NOPE.TXT"), "and it says which");
    CHECK(iso9660_open("/BOOT/NOPE/X.CFG") == 0, "a missing directory on the way");
    printed_clear();
    CHECK(iso9660_open("/KERNEL.ELF/X") == 0, "a file used as a directory");
    CHECK(printed_contains("is not a directory"), "and it says so");
    CHECK(iso9660_open("/KERNEL.EL") == 0 && iso9660_open("/KERNEL.ELFX") == 0, "names match whole, not by prefix");
}

static void a_read_error_fails_the_open(void)
{
    standard_image();
    iso9660_mount();
    disk_fail_at = 21;
    CHECK(iso9660_open("/BOOT/GRUB/X.CFG") == 0, "a directory sector that cannot be read");
}

/* A directory's size that is not a whole number of sectors - not what a
 * mastering tool writes, but what a damaged disc can say. The reader
 * counted the bytes left down by a sector at a time in an unsigned
 * number, which wrapped past zero to four thousand million, and it went
 * on reading sector after sector looking for the name (journal entry
 * NNN, bug 1). */
static void a_directory_size_short_of_a_sector_ends_the_search(void)
{
    standard_image();
    iso9660_mount();
    /* /BIG says 2048 + 100 bytes: one whole sector, and 100 of the next. */
    both_endian32(sector_at(ROOT) + 2 * 34 + 10, SECTOR + 100);
    disk_reads = 0;
    CHECK(iso9660_open("/BIG/NOWHERE") == 0, "a name that is not there");
    CHECK(disk_reads <= 1 + 2, "read the root and /BIG's two sectors, and stopped");
}

/* A record too short to hold its own fields, or longer than the sector
 * has left: parsing it reads past the record (or the sector) into
 * whatever lies there. Such a sector's records end there (bug 2). */
static void a_damaged_record_ends_its_sector(void)
{
    standard_image();
    iso9660_mount();
    /* GRUB's sector: ".", "..", then X.CFG. "..", 34 bytes, says 10. */
    sector_at(22)[34] = 10;
    CHECK(iso9660_open("/BOOT/GRUB/X.CFG") == 0, "a record shorter than its fields: the sector's records end");

    standard_image();
    iso9660_mount();
    /* The last record of /BIG's first sector says it runs 200 bytes
     * past the sector's end. */
    uint8_t *big = sector_at(23);
    uint32_t at = 0, last = 0;
    while (at < SECTOR && big[at]) {
        last = at;
        at += big[at];
    }
    big[last] = (uint8_t)(SECTOR - last + 200 > 255 ? 255 : SECTOR - last + 200);
    CHECK(SECTOR - last < 255, "the test's own setup: the last record can be made too long");
    CHECK(iso9660_open("/BIG/LAST.TXT") != 0, "the next sector is still read: a damaged record ends its own sector only");

    standard_image();
    iso9660_mount();
    /* A name longer than its record: BIG's, in the root, says 200. The
     * root's records end there, so KERNEL.ELF after it is not reached. */
    sector_at(ROOT)[34 + 34 + 32] = 200;
    CHECK(iso9660_open("/KERNEL.ELF") == 0, "a name longer than its record ends the sector's records");
}

/* A path name longer than the reader's 64-byte buffer for one name: it
 * was copied in whole, over the rest of the stack (bug 3). Refused now. */
static void a_long_name_in_a_path_is_refused(void)
{
    standard_image();
    iso9660_mount();
    char path[160];
    path[0] = '/';
    for (int i = 1; i < 150; i++) {
        path[i] = 'A';
    }
    path[150] = 0;
    printed_clear();
    CHECK(iso9660_open(path) == 0, "a 149-character name");
    CHECK(printed_contains("name too long"), "and it says why");

    char longest[70];
    longest[0] = '/';
    for (int i = 1; i < 64; i++) {
        longest[i] = 'B';
    }
    longest[64] = 0;
    printed_clear();
    CHECK(iso9660_open(longest) == 0 && !printed_contains("name too long"), "63 characters fit: looked for, not refused");
}

static void reads_any_part_of_a_file(void)
{
    standard_image();
    iso9660_mount();
    vfs_node_t *kernel = iso9660_open("/KERNEL.ELF");
    if (!kernel) {
        CHECK(0, "the kernel file opens");
        return;
    }
    uint8_t *buffer = allocate(3 * SECTOR);
    int whole = 1;

    set_bytes(buffer, 0xEE, 3 * SECTOR);
    CHECK(iso9660_read(kernel, buffer, KERNEL_SIZE, 0) == KERNEL_SIZE, "the whole file, three sectors");
    for (uint32_t i = 0; i < KERNEL_SIZE; i++) {
        whole &= buffer[i] == pattern(30, i);
    }
    CHECK(whole, "every byte from its place");
    CHECK(buffer[KERNEL_SIZE] == 0xEE, "nothing written past what was asked");

    /* Across a sector boundary, from the middle of one. */
    set_bytes(buffer, 0xEE, 3 * SECTOR);
    CHECK(iso9660_read(kernel, buffer, 100, SECTOR - 30) == 100, "100 bytes across the first boundary");
    whole = 1;
    for (uint32_t i = 0; i < 100; i++) {
        whole &= buffer[i] == pattern(30, SECTOR - 30 + i);
    }
    CHECK(whole, "the right 100");

    CHECK(iso9660_read(kernel, buffer, 1000, KERNEL_SIZE - 10) == 10, "a read past the end is cut at the end");
    CHECK(iso9660_read(kernel, buffer, 10, KERNEL_SIZE) == 0, "from the end: nothing");
    CHECK(iso9660_read(kernel, buffer, 10, KERNEL_SIZE + 5000) == 0, "from past the end: nothing");
    CHECK(iso9660_read(0, buffer, 10, 0) == -1 && iso9660_read(kernel, 0, 10, 0) == -1, "no node or no buffer: an error");
    /* offset + size past 4 GiB: cut at the end, not wrapped to a small size. */
    CHECK(iso9660_read(kernel, buffer, 0xFFFFFFFFu, 10) == KERNEL_SIZE - 10, "offset + size past 4 GiB is cut at the file's end, not wrapped small");

    disk_fail_at = 31;
    CHECK(iso9660_read(kernel, buffer, KERNEL_SIZE, 0) == -1, "a sector that cannot be read: an error");
}

static void a_real_iso(const char *path)
{
    if (!path) {
        out("iso: no real ISO given, its checks skipped\n");
        return;
    }
    uint32_t size = 0;
    uint8_t *real = read_file(path, &size);
    CHECK(real != 0, "the build's ISO can be read");
    if (!real) {
        return;
    }
    disk_use(real, size / SECTOR);
    printed_clear();
    CHECK(iso9660_mount() == 0, "the build's own ISO mounts");
    /* Stage 2 loads it by this name (stage2.asm's kernel_name). */
    vfs_node_t *kernel = iso9660_open("KERNEL/KERNEL.ELF");
    CHECK(kernel && !kernel->is_directory && kernel->size > 4096, "its kernel is found");
    if (kernel) {
        uint8_t *head = allocate(SECTOR);
        CHECK(iso9660_read(kernel, head, 4, 0) == 4 && head[0] == 0x7F && head[1] == 'E', "and begins as an ELF file");
    }
    /* No RAMDISK.MDR: the 64-bit ISO has none (iso64 in TheTaaJ's
     * Makefile copies stage 1, stage 2 and the kernel), and stage 2 goes
     * on without it ("Ramdisk load failed, continuing without one"). */
}

void iso_tests(const char *real_iso)
{
    layout_matches_the_standard();
    mounts_a_good_volume();
    refuses_what_is_not_iso9660();
    opens_the_root_and_files_in_it();
    walks_nested_directories();
    finds_a_name_in_a_directory_s_second_sector();
    says_what_is_missing();
    a_read_error_fails_the_open();
    a_directory_size_short_of_a_sector_ends_the_search();
    a_damaged_record_ends_its_sector();
    reads_any_part_of_a_file();
    a_real_iso(real_iso);
    /* Last: before its fix it overran the stack, and the run ended. */
    a_long_name_in_a_path_is_refused();
}
