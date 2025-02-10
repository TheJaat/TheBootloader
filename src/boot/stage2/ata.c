#include <port_io.h>
#include <print.h>
#include <ata.h>
#include <mem.h>
#include <ISO9660.h>
#include <constants.h>

#include <vfs.h>

/*
* This global variable stores the kernel size,
* It is used in stage2.asm as an external.
* It's value then stored in OsBootDescriptor structure.
*/
// moved to vfs.c
// int g_kernelSize;

#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

#define ATA_PRIMARY_IO_BASE 0x1F0


#define IDE_ATA        0x00
#define IDE_ATAPI      0x01
 
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_SR_BSY     0x80

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC

struct ata_device ata_primary_master = {
    .io_base = 0x1F0,
    .control = 0x3F6,
    .slave = 0
};

struct ata_device ata_primary_slave = {
    .io_base = 0x1F0,
    .control = 0x3F6,
    .slave = 1
};

struct ata_device ata_secondary_master = {
    .io_base = 0x170,
    .control = 0x376,
    .slave = 0
};

struct ata_device ata_secondary_slave = {
    .io_base = 0x170,
    .control = 0x376,
    .slave = 1
};


#define ATA_REG_ALTSTATUS  0x0C

void ata_io_wait(struct ata_device * dev) {
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
}

// Function to perform an ATA soft reset
void ata_soft_reset(struct ata_device *dev) {
    // Write the reset bit (SRST) to the Device Control Register
    outb(dev->control, 0x04);
    // Wait for at least 4 microseconds
    ata_io_wait(dev);
    // Clear the reset bit
    outb(dev->control, 0x00);
    
    // Wait for the device to become ready
//    while ((inb(dev->io_base + 7) & 0x80) != 0) {
        // Wait while the BSY (busy) bit is set
//    }

    // Optionally, wait for DRDY (device ready) bit to be set
//    while ((inb(dev->io_base + 7) & 0x40) == 0) {
        // Wait until the DRDY bit is set
//    }
}

int ata_wait(struct ata_device * dev, int advanced) {
	unsigned char status = 0;

	ata_io_wait(dev);

	status = ata_status_wait(dev, -1);

	if (advanced) {
		status = inb(dev->io_base + ATA_REG_STATUS);
		if (status   & ATA_SR_ERR)  return 1;
		if (status   & ATA_SR_DF)   return 1;
		if (!(status & ATA_SR_DRQ)) return 1;
	}

	return 0;
}

int ata_status_wait(struct ata_device * dev, int timeout) {
    int status;
    if (timeout > 0) {
        int i = 0;
        while ((status = inb(dev->io_base + ATA_REG_STATUS)) & ATA_SR_BSY && (i < timeout)) i++;
    } else {
        while ((status = inb(dev->io_base + ATA_REG_STATUS)) & ATA_SR_BSY);
    }
    return status;
}

void detect_device_ata_atapi(struct ata_device *dev) {
	ata_soft_reset(dev);
	ata_io_wait(dev);

	// Select the drive: 0xA0 for master, 0xB0 for slave
	outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	
	// Wait for the drive to select
	ata_io_wait(dev);
	ata_status_wait(dev, 10000);
	
	// Read the signature bytes
	unsigned char cl = inb(dev->io_base + ATA_REG_LBA1);
	unsigned char ch = inb(dev->io_base + ATA_REG_LBA2);

	// Determine device type based on signature
	if (cl == 0xFF && ch == 0xFF) {
		// No device present
		boot_print("[ATA] No Device Connected to this Channel.\n");
		dev->is_device_connected = 0;
		return;
	}

	if ((cl == 0x00 & ch == 0x00) || (cl == 0x3C && ch == 0xC3))
	{
		// Parallel ATA Device \\ Emulated SATA
		// SATA hardware appear as legacy IDE hardware
		boot_print("[ATA] ATA Device Connected to this Channel.\n");
		dev->is_device_connected = 1; // Set device connected flag for this device.
		dev->is_atapi = 0;			  // Not an atapi device

		// Send IDENTIFY command
		outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
		ata_io_wait(dev);

		//  Get 512 identity bytes
		uint16_t *identify_data = (uint16_t *)&dev->identity;
		for (int i = 0; i < 256; i++)
		{
			identify_data[i] = inw(dev->io_base + ATA_REG_DATA);
		}

		unsigned char *ptr = (unsigned char *)&dev->identity.model;
		for (int i = 0; i < 39; i += 2)
		{
			unsigned char tmp = ptr[i + 1];
			ptr[i + 1] = ptr[i];
			ptr[i] = tmp;
		}

		// Print Identity data
		boot_print("[ATA] Model: ");
		boot_print(ptr);
		boot_print("\n");
		boot_print("[ATA] Flags: ");
		boot_print_hex(dev->identity.flags);
		boot_print("\n");
		boot_print("[ATA] Serial: ");
		boot_print_hex(dev->identity.serial);
		boot_print("\n");
		boot_print("[ATA] Firmware Revision: ");
		boot_print_hex(dev->identity.firmware);
		boot_print("\n");
		boot_print("[ATA] Sectors Per Interrupt: ");
		boot_print_hex(dev->identity.sectors_per_int);
		boot_print("\n");
		return;
	}
	// It is an ATAPI device.
	if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96))
	{
		// ATAPI device
		boot_print("[ATA] ATAPI Device Connected to this Channel.\n");

		// set device connected flag
		dev->is_device_connected = 1;

		// yes, it is an atapi device
		dev->is_atapi = 1;

		// 
		outb(dev->io_base + 1, 1);
		outb(dev->control, 0);

		outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
		ata_io_wait(dev);

		outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
		ata_io_wait(dev);

		ata_wait(dev, 0);

		// read 512 bytes identity data
		uint16_t *buf = (uint16_t *)&dev->identity;
		for (int i = 0; i < 256; ++i)
		{
			buf[i] = inw(dev->io_base);
		}

		// ATAPI devices, as well as many other ATA devices, store strings
		//  in a somewhat unusual format: each pair of characters is stored
		//  in reverse byte order. This is often referred to as "big-endian"
		//  format for the strings. The code snippet corrects this
		//  byte order to make the model string human-readable.
		unsigned char *ptr = (unsigned char *)&dev->identity.model;
		for (int i = 0; i < 39; i += 2)
		{
			/*
				In each iteration, the code swaps the bytes of each character pair.
				For example, if the model field contains the bytes 0x41 0x42 0x43 0x44
				(which would represent the string "ABCD" in reversed pairs), the loop
				will swap these bytes to 0x42 0x41 0x44 0x43, resulting in "BADC".
			*/
			unsigned char tmp = ptr[i + 1];
			ptr[i + 1] = ptr[i];
			ptr[i] = tmp;
		}
		boot_print("[ATAPI] Model: ");
		boot_print(ptr);
		boot_print("\n");

		/* Detect medium */
		atapi_command_t command;

		// clear | reset, the command union
		memsetb(&command, 0, sizeof(command));
		command.command_bytes[0] = 0x25;

		uint16_t bus = dev->io_base;

		// Send the PACKET COMMAND to the device
		outb(bus + ATA_REG_FEATURES, 0x00);
		outb(bus + ATA_REG_LBA1, 0x08);		// ATAPI packet size (upper byte)
		outb(bus + ATA_REG_LBA2, 0x08);		// ATAPI packet size (lower byte)
		outb(bus + ATA_REG_COMMAND, ATA_CMD_PACKET);

		// poll the status register
		while (1)
		{
			unsigned char status = inb(dev->io_base + ATA_REG_STATUS);
			if ((status & ATA_SR_ERR))
				goto atapi_error;
			if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY))
				break;
		}

		// Send the command packet
		for (int i = 0; i < 6; ++i)
		{
			outw(bus, command.command_words[i]);
		}

		// poll the status register again
		while (1)
		{
			unsigned char status = inb(dev->io_base + ATA_REG_STATUS);
			if ((status & ATA_SR_ERR))
				goto atapi_error_read;
			if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY))
				break;
			if ((status & ATA_SR_DRQ))
				break;
		}

		// read any returned data
		// read 4 (16 bits)= 8 byte data from port
		uint16_t data[4];
		for (int i = 0; i < 4; ++i)
		{
			data[i] = inw(bus);
		}

		uint32_t lba, blocks;

		// Copy the first 4 byte = 32 bits (2*16 bits) from data to lba, and 
		// then convert it in big-endian format from little-endian.
		memcpyb(&lba, &data[0], sizeof(uint32_t));
		lba = LittleToBigEndian(lba);

		// Copy the next 4 byte  = 32 bits from data to blocks, and
		// then convert it in big-endian format from little-endian.
		memcpyb(&blocks, &data[2], sizeof(uint32_t));
		blocks = LittleToBigEndian(blocks);

		// Store the converted LBA
		dev->atapi_lba = lba;

		// Store the converted Block Size
		dev->atapi_sector_size = blocks;

		boot_print("[ATAPI], Block Size: ");
		boot_print_hex(dev->atapi_sector_size);
		boot_print("\n");

		return;

	atapi_error_read:
		return;

	atapi_error:
		return;
	}
}

void detect_ata_devices(){
	boot_print("[ATA] Detecting Devices\n");
	// Primary Channel, master device
	detect_device_ata_atapi(&ata_primary_master);
	// Primary Channel, slave device
	detect_device_ata_atapi(&ata_primary_slave);
	// Secondary Channel, master device
	detect_device_ata_atapi(&ata_secondary_master);
	// Secondary Channel, slave device
	detect_device_ata_atapi(&ata_secondary_slave);
	boot_print("[ATA] Detecting Devices Done\n");

	// This code snippet read the kernel of size 26 sectors (512 bytes each) from the disk.img at LBA 59
	// let's read the kernel from the primary channel, master device
	/*
	unsigned int lba = 59;
	int sector_count = 26;
	ata_read_sectors(&ata_primary_master, lba, sector_count, 0xB000);
	*/

}

// Find and load the kernel, using a helper function.
int find_and_load_kernel_from_9660_using_atapi(const char* kernel_name)
{
	boot_print("Kernel Name to look for: ");
	boot_print(kernel_name);
	boot_print("\n");

	if (check_and_load_kernel(&ata_primary_master, kernel_name)) {
        return 1; // Kernel loaded, exit early
    }
    if (check_and_load_kernel(&ata_primary_slave, kernel_name)) {
        return 1; // Kernel loaded, exit early
    }
    if (check_and_load_kernel(&ata_secondary_master, kernel_name)) {
        return 1; // Kernel loaded, exit early
    }
    if (check_and_load_kernel(&ata_secondary_slave, kernel_name)) {
        return 1; // Kernel loaded, exit early
    }

	// kernel not loaded
	return 0;		// return false
}

// int check_and_set_atapi_device()
// {
// 	boot_print("[ATA] check_and_set_atapi_device, setting ATAPI disk\n");
// 	int status = 0;    // false
// 	if (check_for_atapi(&ata_primary_master) == 1)
// 	{
// 		status = 1;
// 	}
// 	if (check_for_atapi(&ata_primary_slave) == 1)
// 	{
// 		return 1;
// 	}

// 	if (check_for_atapi(&ata_secondary_master) == 1)
// 	{
// 		return 1;
// 	}
// 	if (check_for_atapi(&ata_secondary_slave) == 1)
// 	{
// 		return 1;
// 	}

// 	// Didn't found the ATAPI device
// 	return 0;
// }

int check_and_set_atapi_device() {
    boot_print("[ATA] Scanning for ATAPI devices...\n");

	// Use an array of pointers instead of an array of variables
    struct ata_device *ata_devices[] = {
        &ata_primary_master,
        &ata_primary_slave,
        &ata_secondary_master,
        &ata_secondary_slave
    };

    for (size_t i = 0; i < sizeof(ata_devices) / sizeof(ata_devices[0]); i++) {
		if(!ata_devices[i]->is_atapi){
			;continue;
		}
        if (check_for_atapi(ata_devices[i]) == 1) {
            boot_print("[ATA] ATAPI device detected.\n");
            return 1;  // Found and set an ATAPI device
        }
    }

    boot_print("[ATA] No valid ATAPI device found.\n");
    return 0;  // No ATAPI device detected
}

int check_for_atapi(struct ata_device *_device)
{
	if (device->is_atapi)
	{
		// Store the current device in the global variable
		device = _device;
		boot_print("sector size = "); boot_print_hex(device->atapi_sector_size); boot_print("\n");

		// Check for the ATAPI device sector size, it should be 2048 = 0x800
		if (device->atapi_sector_size != 2048)
		{
			boot_print_hex(device->atapi_sector_size);
			boot_print("\n - bad sector size\n");
			return 0; // return false
		}
		return 1; // return true
	}
	return 0; // return false
}

// Find and load the Kernel, helper function
int check_and_load_kernel(struct ata_device *device, const char* kernel_name) {
    if (device->is_atapi) {
        int status = check_for_atapi(device);//read_atapi_device(device);
		boot_print("Preparing to initialize VFS\n");
init_vfs();
        if (status) {
			return status;
            // return load_kernel_from_iso9660_using_atapi(kernel_name);
        }
    }
	return 0;		// return false
}


void restore_root(void) {
	memcpyb(dir_entry, (iso_9660_directory_entry_t *)&root->root, sizeof(iso_9660_directory_entry_t));

#if 0
	boot_print("Root restored.");
	boot_print("\n Entry len:  "); boot_print_hex( dir_entry->length);
	boot_print("\n File start: "); boot_print_hex( dir_entry->extent_start_LSB);
	boot_print("\n File len:   "); boot_print_hex( dir_entry->extent_length_LSB);
	boot_print("\n");
#endif
}

int _read_12 = 0;
void ata_device_read_sector_atapi(struct ata_device * dev, uint32_t lba, uint8_t * buf) {
boot_print("[ATA] ata_device_read_sector_atapi: Reading\n"); boot_print_hex(lba); boot_print("\n");
	if (!dev->is_atapi) return;


	uint16_t bus = dev->io_base;

_try_again:
boot_print("[ATA] ata_device_read_sector_atapi: trying\n");
	outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	ata_io_wait(dev);

	outb(bus + ATA_REG_FEATURES, 0x00);
	outb(bus + ATA_REG_LBA1, dev->atapi_sector_size & 0xFF);
	outb(bus + ATA_REG_LBA2, dev->atapi_sector_size >> 8);
	outb(bus + ATA_REG_COMMAND, ATA_CMD_PACKET);

	/* poll */
	while (1) {
		uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_SR_ERR)) goto atapi_error_on_read_setup;
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
	}

	atapi_command_t command;
	command.command_bytes[0] = _read_12 ? 0xA8 : 0x28;
	command.command_bytes[1] = 0;
	command.command_bytes[2] = (lba >> 0x18) & 0xFF;
	command.command_bytes[3] = (lba >> 0x10) & 0xFF;
	command.command_bytes[4] = (lba >> 0x08) & 0xFF;
	command.command_bytes[5] = (lba >> 0x00) & 0xFF;
	command.command_bytes[6] = 0;
	command.command_bytes[7] = 0;
	command.command_bytes[8] = _read_12 ? 0 : 1; /* bit 0 = PMI (0, last sector) */
	command.command_bytes[9] = _read_12 ? 1 : 0; /* control */
	command.command_bytes[10] = 0;
	command.command_bytes[11] = 0;

	for (int i = 0; i < 6; ++i) {
		outw(bus, command.command_words[i]);
	}

	while (1) {
		uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_SR_ERR)) goto atapi_error_on_read_setup_cmd;
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
	}

	uint16_t size_to_read = inb(bus + ATA_REG_LBA2) << 8;
	size_to_read = size_to_read | inb(bus + ATA_REG_LBA1);

	inwm(bus,buf,size_to_read/2);

	while (1) {
		uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_SR_ERR)) goto atapi_error_on_read_setup;
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) break;
	}

	return;

atapi_error_on_read_setup:
	boot_print("[ATA] ata_device_read_sector_atapi: Error on setup\n");
	return;
atapi_error_on_read_setup_cmd:
	if (_read_12) {
		_read_12 = 0;
		boot_print("Trying again\n");
		goto _try_again;
	}
	boot_print("[ATA] ata_device_read_sector_atapi: Error on cmd\n");
	return;
}


void ata_read_sectors(struct ata_device *dev, unsigned int lba, int sector_count, unsigned short* buffer) {
	// Select the drive and LBA mode
	outb(dev->io_base + ATA_REG_HDDEVSEL, 0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F));
	ata_io_wait(dev);

	// Send the sector count
	outb(dev->io_base + ATA_REG_SECCOUNT0, sector_count);

	// Send the LBA address
	outb(dev->io_base + ATA_REG_LBA0, (unsigned char)lba);
	outb(dev->io_base + ATA_REG_LBA1, (unsigned char) (lba >> 8));
	outb(dev->io_base + ATA_REG_LBA2, (unsigned char) (lba >> 16));

	// Send the Read Command
	outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
	ata_io_wait(dev);

	// Read the sector into buffer
	for (int sector = 0; sector< sector_count; sector++){
		ata_status_wait(dev, 10000);
		for (int i = 0; i < 256; i++) {
			((unsigned short *)buffer)[(sector*256) + i] = inw(dev->io_base + ATA_REG_DATA);
		}
	}
}

int read_sector(unsigned int lba, unsigned char* buffer) {
    boot_print("[ATA] Reading Sector\n");
    ata_device_read_sector_atapi(device, lba, buffer);
    return 0;
}

/*
void ata_read_sector() {

	unsigned int lba = 59;
	int sector_count = 26;

	unsigned short *buffer = (unsigned short *)0xB000;

	// Select the drive and head
	outb(ATA_PRIMARY_IO_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F)); 
	// Set the sector count to 1
	outb(ATA_PRIMARY_IO_BASE + 2, 1);  
	// Set the sector number
	outb(ATA_PRIMARY_IO_BASE + 3, lba); 
	// Set the cylinder low
	outb(ATA_PRIMARY_IO_BASE + 4, lba >> 8); 
	// Set the cylinder high
	outb(ATA_PRIMARY_IO_BASE + 5, lba >> 16); 
	// Send the read command
	outb(ATA_PRIMARY_IO_BASE + ATA_REG_COMMAND, ATA_READ_SECTORS);

	// Wait for the device to be ready
	while (!(inb(ATA_PRIMARY_IO_BASE + 7) & 0x08));

	// Read data from the data register
	for (int sector = 0; sector< sector_count; sector++){
		for (int i = 0; i < 256; i++) {
			((unsigned short *)buffer)[(sector*256) + i] = inw(ATA_PRIMARY_IO_BASE);
		}
	}
}
*/

// Read the atapi device, Validate its PVD (Primary Volume Descriptor)
int read_atapi_device(struct ata_device * _device) {
//boot_print("Read ATAPI Device");
	// Store the current device in the global variable
	device = _device;

	// Check for the ATAPI device sector size, it should be 2048 = 0x800
	if (device->atapi_sector_size != 2048) {
		boot_print_hex(device->atapi_sector_size);
		boot_print("\n - bad sector size\n");
		return	0;                        // return false
	}
boot_print("Preparing to initialize VFS\n");
init_vfs();


	// Read volume descriptors of ISO9660
	// start from LBA 16 which is block 16 (0-indexed)
//	for (int i = 0x10; i < 0x15; ++i) {
		// Read this block
//		ata_device_read_sector_atapi(device, i, (uint8_t *)root);

		// Check for the descriptor type
//		switch (root->type) {
//			case 1:	// Its Primary Volume Descriptor
//				root_sector = i;
//				goto done;
//			case 0xFF:	// its the Volume Set Termination Descriptor that is the end of the descriptors.
//				return 0;	// return false
//		}
//	}
//	return 0;	// return false
//done:
//	restore_root();
	return 1;	// return true
}

// Find and load the kernel at destined location, KERNEL_LOAD_START
int load_kernel_from_iso9660_using_atapi(const char* kernel_name)
{
	// Traverse the disk print all its files/ directories.
	traverse_the_disk();

	// Our kernel is "kernel_entry.bin" however, ISO9660 only allows the idenfier to be
	// of length 11 excluding (.) with 8:3 (8 character for the name without extension,
	// and 3 characters for the extension).
	if (navigate_to_file_directory_entry(kernel_name)) {//("KERNEL_E.BIN")) {
		boot_print("Found the kernel.\n");
		boot_print("Kernel Start Extent LSB = "); boot_print_hex(searched_file_dir_entry->extent_start_LSB); boot_print("\n");
		boot_print("Kernel Size (in bytes, hex)= "); boot_print_hex(searched_file_dir_entry->extent_length_LSB); boot_print("\n");

		// Store the kernel size in global variable which would be access in stage2.asm
		// moved to vfs.c
		// g_kernelSize = searched_file_dir_entry->extent_length_LSB;

		long offset = 0;
		for (int i = searched_file_dir_entry->extent_start_LSB; i < searched_file_dir_entry->extent_start_LSB + searched_file_dir_entry->extent_length_LSB / 2048 + 1; ++i, offset += 2048) {
			ata_device_read_sector_atapi(device, i, (uint8_t *)KERNEL_LOAD_START + offset);
		}
		boot_print("Kernel was loaded successfully.\n");


		// Below snippet is for calling the loaded kernel using c code.
		// uint32_t kernel_address = 0x300000;
		// void (*kernel_entry)(void) = (void(*)(void))0x300000;
		// kernel_entry();
		// restore_root();
		return 1;		// return true
	} else {
		boot_print("Kernel was not found.\n");
		return 0;		// return false
	}

}

void jump_to_kernel()
{
	uint32_t kernel_address = KERNEL_LOAD_START;
	void (*kernel_entry)(void) = (void (*)(void))0x300000;
	kernel_entry();
}

