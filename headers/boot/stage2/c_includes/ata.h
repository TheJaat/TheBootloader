#ifndef _ATA_IO_H
#define _ATA_IO_H
#include <mem.h>

// This macro converts 32 bits from little endian byte order to big endian
#define LittleToBigEndian(l) ((((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))

// Identity structure = 512 bytes
typedef struct {
    uint16_t flags;                // General configuration (word 0)
    uint16_t unused1[9];           // Reserved fields (words 1-9)
    char     serial[20];           // Serial number (words 10-19)
    uint16_t unused2[3];           // Reserved fields (words 20-22)
    char     firmware[8];          // Firmware revision (words 23-26)
    char     model[40];            // Model number (words 27-46)
    uint16_t sectors_per_int;      // Number of sectors per interrupt on R/W multiple (word 47)
    uint16_t unused3;              // Reserved field (word 48)
    uint16_t capabilities[2];      // Capabilities (words 49-50)
    uint16_t unused4[2];           // Reserved fields (words 51-52)
    uint16_t valid_ext_data;       // Field validity (word 53)
    uint16_t unused5[5];           // Reserved fields (words 54-58)
    uint16_t size_of_rw_mult;      // Number of current sectors for R/W multiple (word 59)
    uint32_t sectors_28;           // Total number of user-addressable sectors (28-bit) (words 60-61)
    uint16_t unused6[38];          // Reserved fields (words 62-99)
    uint64_t sectors_48;           // Total number of user-addressable sectors (48-bit) (words 100-103)
    uint16_t unused7[152];         // Reserved fields (words 104-255)
} __attribute__((packed)) ata_identify_t;


// Structure for the ata device
struct ata_device {
    int io_base;                  // Base I/O port address for the ATA device
    int control;                  // Control I/O port address
    int slave;                    // Indicates if the device is a slave (0 for master, 1 for slave)
    int is_atapi;                 // Indicates if the device is ATAPI (0 for ATA, 1 for ATAPI)
    ata_identify_t identity;      // Structure holding the identity information of the device
    unsigned int atapi_lba;       // Logical Block Addressing (LBA) for ATAPI devices
    unsigned int atapi_sector_size; // Sector size for ATAPI devices in bytes
    int is_device_connected;	// If the device is connected to this channel.
};


typedef union {
	uint8_t command_bytes[12];
	uint16_t command_words[6];
} atapi_command_t;


// Declaration of functions
int ata_read_sector();

int read_sector(unsigned int lba, unsigned char* buffer);

#endif // _ATA_IO_H
