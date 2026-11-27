#ifndef _BIOS_DISK_H_
#define _BIOS_DISK_H_

/* One ISO 9660 sector (2048 bytes), number `lba`, of the boot disk,
 * read through the BIOS into `buffer`. 0, or -1 if the BIOS could not. */
int read_sector(unsigned int lba, unsigned char *buffer);

/* `count` of them, from `lba`, into `buffer` (count * 2048 bytes): many
 * per BIOS call (K131). 0, or -1 if the BIOS could not read one. */
int read_sectors(unsigned int lba, unsigned int count, unsigned char *buffer);

#endif
