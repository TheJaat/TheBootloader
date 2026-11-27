/*
 * bios_disk.c - reading the boot disk through the BIOS (int 13h).
 *
 * The loader reads its files from the disk the BIOS booted from, the
 * way the BIOS reads it: int 13h, function 42h ("extended read"), by
 * sector number. That one call works for every kind of boot disk the
 * BIOS knows - a CD (El Torito: 2048-byte sectors), a USB stick or a
 * hard disk (512-byte sectors) - whatever the controller behind it: IDE,
 * SATA (AHCI), USB, NVMe. The BIOS has a driver for each; this loader
 * then needs none.
 *
 * int 13h is a real-mode call and this code runs in 32-bit protected
 * mode, so each read goes through `bios_read` (stage2.asm): down to real
 * mode, the call, back up. The BIOS can only put the sectors below
 * 1 MiB, in BIOS_BUFFER; they are copied from there to where they go.
 *
 * The file system is ISO 9660 either way - on a stick, the image is a
 * CD image written to it - so a "sector" here is ISO 9660's, 2048
 * bytes: four of a stick's.
 */

#include <stdint.h>
#include <print.h>
#include <mem.h>

/* stage2.asm: reads `count` sectors of the boot disk from `lba` into
 * BIOS_BUFFER; 0, or the BIOS's error code. */
extern unsigned int bios_read(unsigned int lba, unsigned int count);
/* stage2.asm: the boot disk's sector size, asked of the BIOS (int 13h,
 * function 48h) in real mode. */
extern unsigned short wDiskSectorSize;

#define BIOS_BUFFER ((unsigned char *)0x60000)
#define ISO_SECTOR 2048

/* The most ISO sectors one int 13h call reads (K131). Every call costs
 * the trip to real mode and back - cheap on a PC, expensive emulated:
 * one sector at a time, the kernel's 830 sectors took most of an
 * emulated boot (K51, entry 105). Sixteen is 32 KiB: within the BIOS
 * buffer (0x60000 up to the ISO reader's buffers at 0x80000), inside one
 * 64 KiB block, as some BIOSes need for their DMA, and 64 of a stick's
 * 512-byte sectors - below the 127 many BIOSes take at most per call. */
#define READ_MAX 16

/* `count` ISO sectors (2048 bytes each) of the boot disk from `lba`, into
 * `buffer`: as few BIOS calls as READ_MAX allows. 0, or -1 if the BIOS
 * could not read one of them. */
int read_sectors(unsigned int lba, unsigned int count, unsigned char *buffer) {
    unsigned int per = ISO_SECTOR / (wDiskSectorSize == 512 ? 512 : ISO_SECTOR);
    while (count > 0) {
        unsigned int n = count < READ_MAX ? count : READ_MAX;
        int read = 0;
        /* Twice before giving up: the first read of a USB device sometimes
         * fails while the BIOS's driver is still waking it. */
        for (int attempt = 0; attempt < 2 && !read; attempt++) {
            read = bios_read(lba * per, n * per) == 0;
        }
        if (!read) {
            boot_print("[DISK] the BIOS could not read sectors from ");
            boot_print_hex(lba);
            boot_print("\n");
            return -1;
        }
        memcpy(buffer, BIOS_BUFFER, (int)(n * ISO_SECTOR));
        buffer += n * ISO_SECTOR;
        lba += n;
        count -= n;
    }
    return 0;
}

int read_sector(unsigned int lba, unsigned char *buffer) {
    return read_sectors(lba, 1, buffer);
}
