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
//    iso_fs.root_dir_sector = *(unsigned int *)(pvd->root + 2);
//    iso_fs.root_dir_size = *(unsigned int *)(pvd->root + 10);

    // Read Successfully
    // Copy the RootDirectory Entry to the location 0x20800
    // memcpyb(root_dir_entry, (iso_9660_directory_entry_t *)&root->root, sizeof(iso_9660_directory_entry_t));

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

// Returns a pointer to a newly allocated vfs_node_t if found,
// or NULL if not found.
vfs_node_t* iso9660_find_in_directory(unsigned int sector, unsigned int size, const char* name) {
    boot_print("[ISO9660-FS] Searching for entry: ");
    boot_print(name);
    boot_print("\n");
    
    unsigned int current_sector = sector;
    unsigned int remaining_bytes = size;
    
    // Scan through all sectors that contain the directory records.
    while (remaining_bytes > 0) {
        if (read_sector(current_sector, root_dir_entries) != 0)
            return (void*)0;
        
        unsigned int offset = 0;
        // Process records in the current sector.
        while (offset < ISO9660_SECTOR_SIZE && offset < remaining_bytes) {
            iso_9660_directory_entry_t *dir = 
                (iso_9660_directory_entry_t *)(root_dir_entries + offset);
            
            // If length is 0, we've hit the end of valid records in this sector.
            if (dir->length == 0) {
                break;
            }
            
            // Build a null-terminated string for the file/directory name.
            char file_name[128];  // Adjust size as needed.
            int name_len = (dir->name_len < sizeof(file_name) - 1) ? dir->name_len : (sizeof(file_name) - 1);
            memcpyb(file_name, dir->name, name_len);
            file_name[name_len] = '\0';
            
            // Remove version information (e.g., ";1") if present.
            char *s = strchr(file_name, ';');
            if (s) {
                *s = '\0';
            }
            
            boot_print("[ISO9660-FS] Found entry: ");
            boot_print(file_name);
            boot_print("\n");
            
            // Compare the entry name with the requested name.
            if (strcmp(file_name, name) == 0) {
                boot_print("[ISO9660-FS] Entry matched: ");
                boot_print(file_name);
                boot_print("\n");
                
                // Allocate and populate a new vfs_node_t for the found entry.
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
            
            // Move to the next directory record within the sector.
            offset += dir->length;
        }
        // Move on to the next sector in the directory data.
        current_sector++;
        remaining_bytes -= ISO9660_SECTOR_SIZE;
    }
    return (void*)0;
}

// Main function: Opens a file or directory by path.
// Supports paths with multiple components (e.g., "/DIR/SUBDIR/FILE.TXT").
vfs_node_t* iso9660_open(const char* path) {
    boot_print("[ISO9660-FS] iso9660_open: path = ");
    boot_print(path);
    boot_print("\n");
    
    // Start with the root directory information.
    // (Assume iso_fs has been populated during mount.)
    vfs_node_t* current = (vfs_node_t*) stage2_malloc(sizeof(vfs_node_t));
    if (!current) {
        boot_print("Out of memory\n");
        return (void*)0;
    }
    strncpy(current->name, "/", sizeof(current->name));
    current->sector = iso_fs.root_dir_sector;
    current->size = iso_fs.root_dir_size;
    current->is_directory = 1;
    current->next = (void*)0;
    
    // Skip any leading '/' characters.
    while (*path == '/')
        path++;
    
    // If the path is empty (i.e., just "/"), return the root node.
    if (*path == '\0') {
        return current;
    }
    
    // Tokenize the path by '/' and traverse the directory tree.
    char token[64];  // Adjust size if necessary.
    const char* p = path;
    while (*p) {
        int i = 0;
        // Extract the next token from the path.
        while (*p && *p != '/') {
            token[i++] = *p;
            p++;
        }
        token[i] = '\0';
        
        // Skip additional '/' characters.
        while (*p == '/')
            p++;
        
        // Search for the token in the current directory.
        vfs_node_t* child = iso9660_find_in_directory(current->sector, current->size, token);
        if (!child) {
            boot_print("[ISO9660-FS] iso9660_open: Cannot find ");
            boot_print(token);
            boot_print("\n");
            return (void*)0;
        }
        
        // Ensure that if there are more tokens to process, the found node is a directory.
        if (*p != '\0' && !child->is_directory) {
            boot_print("[ISO9660-FS] iso9660_open: ");
            boot_print(child->name);
            boot_print(" is not a directory\n");
            return (void*)0;
        }
        
        // Move down one level in the directory hierarchy.
        current = child;
    }
    
    // Return the final node (file or directory) corresponding to the full path.
    return current;
}

vfs_node_t* iso9660_open_old(const char* path) {
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



void iso9660_list2(const char* path) {
    boot_print("[ISO9660-FS] Listing directory: ");
    boot_print(path);
    boot_print("\n");

    // unsigned char *sector_buffer = (unsigned char *)0x0500; // Buffer for reading sectors
    unsigned int current_sector = iso_fs.root_dir_sector;
    unsigned int remaining_bytes = iso_fs.root_dir_size;

    while (remaining_bytes > 0) {
        if (read_sector(current_sector, root_dir_entries) != 0) {
            boot_print("[ISO9660-FS] Error reading sector\n");
            return;
        }

        unsigned char *ptr = root_dir_entries;
        while (ptr < root_dir_entries + ISO9660_SECTOR_SIZE) {
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


void iso9660_list(const char* path) {
    boot_print("[ISO9660-FS] Listing directory: ");
    boot_print(path);
    boot_print("\n");

    // First, obtain the directory node for the given path.
    // (iso9660_open() must be implemented to traverse the filesystem tree.)
    vfs_node_t *dir_node = iso9660_open(path);
    if (!dir_node) {
        boot_print("[ISO9660-FS] Directory not found: ");
        boot_print(path);
        boot_print("\n");
        return;
    }
    
    // Ensure that the node is actually a directory.
    if (!dir_node->is_directory) {
        boot_print("[ISO9660-FS] Not a directory: ");
        boot_print(path);
        boot_print("\n");
        return;
    }
    
    // Use the directory's starting sector and size.
    unsigned int current_sector = dir_node->sector;
    unsigned int remaining_bytes = dir_node->size;

    // Loop through all sectors that hold the directory records.
    while (remaining_bytes > 0) {
        if (read_sector(current_sector, root_dir_entries) != 0) {
            boot_print("[ISO9660-FS] Error reading sector\n");
            return;
        }

        unsigned char *ptr = root_dir_entries;
        // Process all records in this sector.
        while (ptr < root_dir_entries + ISO9660_SECTOR_SIZE) {
            unsigned char record_length = ptr[0];
            if (record_length == 0) {
                boot_print("[ISO9660-FS] No more records in this sector\n");
                break;
            }

            // The file identifier length is stored at offset 32.
            unsigned char name_length = ptr[32];
            char filename[128] = {0};
            // The file identifier (name) starts at offset 33.
            memcpy(filename, ptr + 33, name_length);
            filename[name_length] = '\0';

            // Remove version information (e.g., ";1") if present.
            char *semicolon = strchr(filename, ';');
            if (semicolon)
                *semicolon = '\0';

            boot_print("[ISO9660-FS] File: ");
            boot_print(filename);
            boot_print("\n");

            // Advance to the next directory record.
            ptr += record_length;
        }

        current_sector++;
        if (remaining_bytes > ISO9660_SECTOR_SIZE)
            remaining_bytes -= ISO9660_SECTOR_SIZE;
        else
            remaining_bytes = 0;
    }
}
