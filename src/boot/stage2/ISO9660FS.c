#include <ISO9660FS.h>
#include <ata.h>
#include <ISO9660.h>
#include <print.h>
#include <mem.h>
#include <vfs.h>

iso9660_fs_t iso_fs;

extern iso_9660_volume_descriptor_t *root;
unsigned char* buffer = (unsigned char*) 0x0500;//0x20000;
iso_9660_directory_entry_t *root_dir_entry = (iso_9660_directory_entry_t *)((uint8_t *)0x80800);
unsigned char * root_dir_entries = (unsigned char *)(0x90000);



int memcmp(const void *s1, const void *s2, int n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    for (int i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
        {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

int iso9660_mount()
{
    boot_print("[ISO9660-FS] Mounting\n");
    // unsigned char buffer[ISO9660_SECTOR_SIZE];
    unsigned char *buffer = (unsigned char *)0x80000;

    // Step 1: Read Primary Volume Descriptor (PVD) at sector 16
    if (read_sector(ISO9660_PVD_SECTOR, buffer) != 0)
    {
        // error in reading
        boot_print("[ISO9660-FS] Unable to read\n");
        return -1;
    }

    // Cast buffer to a structured PVD
    iso_9660_volume_descriptor_t *pvd = (iso_9660_volume_descriptor_t *)buffer;

    // Step 2: Validate ISO9660 signature ("CD001" at offset 1)
    if (memcmp(pvd->id, "CD001", 5) == 0)
    {
        // Valid ISO9660 FS
        boot_print("[ISO9660-FS] Mount:: Root Directory Start Sector = ");
        boot_print_hex(*(unsigned int *)pvd->root + 2);
        boot_print("\n");
    }
    else
    {
        // Not a valid ISO9660 FS
        boot_print("[ISO9660-FS] iso9660_mount: Not a valid ISO9660 FS\n");
        return -1;
    }

// The root directory record is stored in pvd->root (34 bytes)
// Cast it to a proper directory entry structure pointer.
iso_9660_directory_entry_t *root_entry = (iso_9660_directory_entry_t *)pvd->root;

// Step 3: Extract the Root Directory Record Information
// Use the little-endian values from the root directory record
iso_fs.root_dir_sector = root_entry->extent_start_LSB;
iso_fs.root_dir_size = root_entry->extent_length_LSB;

    // Step 3: Extract Root Directory Record (at offset 156 in PVD)
   // iso_fs.root_dir_sector = *(unsigned int *)(pvd->root + 2);
  //  iso_fs.root_dir_size = *(unsigned int *)(pvd->root + 10);

    // Read Successfully
    // Copy the RootDirectory Entry to the location 0x20800
    memcpyb(root_dir_entry, (iso_9660_directory_entry_t *)&root->root, sizeof(iso_9660_directory_entry_t));

    return 0;
}

int nestedDirOffset = 0;
// Opening a File
vfs_node_t* iso9660_open1(const char* path) {

boot_print("[ISO9660-FS] iso9660_open\n");
    // Begin at the root directory.
    // The root directory's starting sector and total size were stored by iso9660_mount.
    unsigned int current_sector = iso_fs.root_dir_sector;
    unsigned int remaining_bytes = iso_fs.root_dir_size;
    boot_print("[ISO9660-FS] iso9660_open: current_sector = "); boot_print_hex(current_sector); boot_print("\n");
    boot_print("[ISO9660-FS] iso9660_open: remaining_bytes = "); boot_print_hex(remaining_bytes); boot_print("\n");


    // Iterate through each sector of the root directory.
    unsigned int offset = 0;
    while(remaining_bytes > 0) {
        // Read one sector of the directory.
        if (read_sector(current_sector, root_dir_entries) != 0)
            return (void *)0;
        while (offset < remaining_bytes)
        {
            iso_9660_directory_entry_t *dir = (iso_9660_directory_entry_t *)(root_dir_entries + offset);

            boot_print("dir->length = ");
            boot_print_hex(dir->length);
            boot_print("\n");
            if (dir->length == 0)
            {
                boot_print("Dead End \n");
               
                break;
            }

            char file_name[dir->name_len + 1];
            memcpyb(file_name, dir->name, dir->name_len);
            file_name[dir->name_len] = 0;
            char *s = strchr(file_name, ';');
            if (s)
            {
                *s = '\0';
            }
            // boot_print(path);
            // boot_print("/");
            boot_print(file_name);
            boot_print("\n");

            // Compare the file.
            if (strcmp(file_name, path) == 0)
            {
                // Found the file.
                boot_print("Found the File\n");
            }
            // If the entry is a directory
            if (dir->flags & FLAG_DIRECTORY)
            {
            }


            offset += dir->length;
        }
    }

    // File not found.
    return (void*)0;
}

vfs_node_t* iso9660_open(const char* path) {
    boot_print("[ISO9660-FS] iso9660_open\n");
    
    // Begin at the root directory.
    unsigned int current_sector = iso_fs.root_dir_sector;
    unsigned int remaining_bytes = iso_fs.root_dir_size;
    boot_print("[ISO9660-FS] iso9660_open: current_sector = "); boot_print_hex(current_sector); boot_print("\n");
    boot_print("[ISO9660-FS] iso9660_open: remaining_bytes = "); boot_print_hex(remaining_bytes); boot_print("\n");

    // Iterate through each sector of the root directory.
    while (remaining_bytes > 0) {
        if (read_sector(current_sector, root_dir_entries) != 0)
            return (void*)0;
        
        unsigned int offset = 0;
        while (offset < ISO9660_SECTOR_SIZE && offset < remaining_bytes) {
            iso_9660_directory_entry_t *dir = (iso_9660_directory_entry_t *)(root_dir_entries + offset);

            if (dir->length == 0) {
                boot_print("Dead End \n");
                break;
            }

            char file_name[dir->name_len + 1];
            memcpyb(file_name, dir->name, dir->name_len);
            file_name[dir->name_len] = '\0';
            
            char *s = strchr(file_name, ';');
            if (s) {
                *s = '\0';
            }

            boot_print(file_name);
            boot_print("\n");

            // Compare the file name with the requested path.
            if (strcmp(file_name, path) == 0) {
                boot_print("Found the File\n");
                
                vfs_node_t* node = (vfs_node_t*) stage2_malloc(sizeof(vfs_node_t));
                if (!node) {
                    boot_print("Out of memory\n");
                    return (void*)0;
                }
                
                strncpy(node->name, file_name, sizeof(node->name));
                node->sector = dir->extent_start_LSB;
                node->size = dir->extent_length_LSB;
                node->is_directory = (dir->flags & FLAG_DIRECTORY) ? 1 : 0;
                node->next = (void*)0;
                return node;
            }

            offset += dir->length;
        }

        // Move to the next sector
        current_sector++;
        remaining_bytes -= ISO9660_SECTOR_SIZE;
    }

    // File not found.
    return (void*)0;
}

int iso9660_read(vfs_node_t* node, void* buffer, unsigned int size, unsigned int offset) {
    if (!node || !buffer) return -1; // Invalid input

    boot_print("[ISO9660-FS] iso9660_read\n");

    // Ensure offset is within file bounds
    if (offset >= node->size) return 0;

    // Adjust size if it exceeds file size
    if (offset + size > node->size) {
        size = node->size - offset;
    }

    unsigned int sector = node->sector + (offset / ISO9660_SECTOR_SIZE);
    unsigned int sector_offset = offset % ISO9660_SECTOR_SIZE;
    unsigned int bytes_read = 0;
    unsigned char sector_buffer[ISO9660_SECTOR_SIZE];

    while (bytes_read < size) {
        if (read_sector(sector, sector_buffer) != 0) {
            boot_print("Error reading sector\n");
            return -1;
        }

        unsigned int chunk_size = ISO9660_SECTOR_SIZE - sector_offset;
        if (chunk_size > (size - bytes_read)) {
            chunk_size = size - bytes_read;
        }

        memcpy((char*)buffer + bytes_read, sector_buffer + sector_offset, chunk_size);
        bytes_read += chunk_size;
        sector++;
        sector_offset = 0; // Only applies to first sector
    }

    return bytes_read;
}



void iso9660_list(const char* path) {
    boot_print("[ISO9660-FS] Listing directory: ");
    boot_print(path);
    boot_print("\n");

    unsigned char *sector_buffer = (unsigned char *)0x0500; // Buffer for reading sectors
    unsigned int current_sector = iso_fs.root_dir_sector;
    unsigned int remaining_bytes = iso_fs.root_dir_size;

    while (remaining_bytes > 0) {
        if (read_sector(current_sector, sector_buffer) != 0) {
            boot_print("[ISO9660-FS] Error reading sector\n");
            return;
        }

        unsigned char *ptr = sector_buffer;
        while (ptr < sector_buffer + ISO9660_SECTOR_SIZE) {
            unsigned char record_length = ptr[0];
            if (record_length == 0) {
                boot_print("[ISO9660-FS] No more records in this sector\n");
                break;
            }

            unsigned char name_length = ptr[32];
            char filename[128] = {0};
            memcpy(filename, ptr + 33, name_length);
            filename[name_length] = '\0';

            char *semicolon = strchr(filename, ';');
            if (semicolon) *semicolon = '\0'; // Remove version number

            boot_print("[ISO9660-FS] File: ");
            boot_print(filename);
            boot_print("\n");

            ptr += record_length;
        }

        current_sector++;
        remaining_bytes -= ISO9660_SECTOR_SIZE;
    }
}
