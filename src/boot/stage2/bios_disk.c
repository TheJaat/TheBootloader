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

int read_sector(unsigned int lba, unsigned char *buffer) {
    unsigned int per = ISO_SECTOR / (wDiskSectorSize == 512 ? 512 : ISO_SECTOR);
    /* Twice before giving up: the first read of a USB device sometimes
     * fails while the BIOS's driver is still waking it. */
    for (int attempt = 0; attempt < 2; attempt++) {
        if (bios_read(lba * per, per) == 0) {
            memcpy(buffer, BIOS_BUFFER, ISO_SECTOR);
            return 0;
        }
    }
    boot_print("[DISK] the BIOS could not read sector ");
    boot_print_hex(lba);
    boot_print("\n");
    return -1;
}
