#include <ata.h>
#include <ISO9660.h>
#include <print.h>

int root_sector = 0;
iso_9660_volume_descriptor_t * root = (iso_9660_volume_descriptor_t *)((uint8_t *)0x20000);
iso_9660_directory_entry_t * dir_entry = (iso_9660_directory_entry_t *)((uint8_t *)0x20800);
iso_9660_directory_entry_t * searched_file_dir_entry = (iso_9660_directory_entry_t *)((uint8_t *)0x40000);
uint8_t * mod_dir = (uint8_t *)0x21000;
uint8_t * dir_entries = (uint8_t *)(0x30000);
struct ata_device * device = 0;
#define BLOCK_SIZE 2048

// used for the nested recursive call to read the directory entry
// to other location than that of parent's directory entry read location.
int nestedOffset = 0;


// Call the traverse with the root
void traverse_the_disk() {

	traverse(dir_entry, "");

}



void traverse(iso_9660_directory_entry_t *dir_entry, const char *path)
{

	unsigned int extent_start = dir_entry->extent_start_LSB;
	unsigned int extent_length = dir_entry->extent_length_LSB;

	/*
	boot_print("[Traversing] extent_start: ");
	boot_print_hex(extent_start);
	boot_print("\n");

	boot_print("[Traversing] extent_length: ");
	boot_print_hex(extent_length);
	boot_print("\n");
	*/


	// Read the directory entries
	// A special global variable nestedOffset is used to read the subdirectory
	// entry into to offset location, so that it would not overwrite the parent's
	// entry
	for (int itr = 0; itr < extent_length; itr += 2048)
	{
		ata_device_read_sector_atapi(device, extent_start + itr / 2048, dir_entries + nestedOffset + itr);

	}


	unsigned int offset = 0;

	while (offset < extent_length)
	{
		iso_9660_directory_entry_t *dir = (iso_9660_directory_entry_t *)(dir_entries + offset + nestedOffset);
		if (dir->length == 0)
		{

			// boot_print("end\n");
			// In case of end of the current directory entries, need to
			// decrement the nestedOffset by Block size, so when get back
			// previous (parent) directory read location.
			nestedOffset -= BLOCK_SIZE;
			break;
		}
		if (!(dir->flags & FLAG_HIDDEN))
		{
			if (dir->name_len > 0 && dir->name_len < 256)
			{
				char file_name[dir->name_len + 1];
				memcpyb(file_name, dir->name, dir->name_len);
				file_name[dir->name_len] = 0;
				char *s = strchr(file_name, ';');
				if (s)
				{
					*s = '\0';
				}
				boot_print(path);
				boot_print("/");
				boot_print(file_name);
				boot_print("\n");


				// If the entry is a directory, recurse into it
				if (dir->flags & FLAG_DIRECTORY)
				{
					if (dir->name_len > 1)
					{
						char new_path[256];
						char file_name[dir->name_len + 1];
						memcpyb(file_name, dir->name, dir->name_len);
						file_name[dir->name_len] = 0;
						char *s = strchr(file_name, ';');
						if (s)
						{
							*s = '\0';
						}
						
						
						// Since we got to call the same function to
						// read the nested directories, it might
						// overwrite the current directory read
						// location, so nestedOffset is used,
						// it increments the read location by
						// block size thus, not overwriting the
						// current directory read location.
						nestedOffset += BLOCK_SIZE;
						
						// Recursively traverse the sub directories.
						traverse(dir, file_name);
					}
				}
			}
		}
		offset += dir->length;
	}
}


// It splits the file/directory path based on the delimiter "/"
// and store the resulting parts into a 2D array parts.
// input string = KERNEL/KERNEL.ELF
// Output count = 2, can be used in loop with i = 0, i<count, thus
//		running for i = 0 and i = 1
 /**
 * Splits the input string by '/' and stores each part in the parts array.
 * The count of parts is stored in the count variable.
 *
 * @param input  The input string to be split.
 * @param parts  The 2D array to store the parts.
 * @param count  The number of parts found in the input string.
 */
#define MAX_PARTS 10
void split_string(const char* input, char parts[MAX_PARTS][128], int *count){

int i = 0, j = 0;
*count = 0;	// Initialize the count to zero.

/*	while (*input != '\0' && *count < MAX_PARTS)
	{
		if(*input == '/') {
			parts[*count][j] = '\0';
			(*count)++;
			j = 0;
		} else {
			parts[*count][j] = *input;
			j++;
		}
		input++;
	}
	if (j > 0) {
		parts[*count][j] = '\0';
		(*count)++;
	}

*/

// Iterate through each character in the input string.
    while (*input != '\0' && *count < MAX_PARTS) {
        if (*input == '/') {
            // When a '/' is encountered, terminate the current part string.
            if (j > 0) {  // Ensure there are characters in the current part.
                parts[*count][j] = '\0';  // Terminate the current part string.
                (*count)++;  // Increment the parts count.
                j = 0;  // Reset the character index for the next part.
            }
        } else {
            // Copy the character to the current part.
            parts[*count][j] = *input;
            j++;
        }
        input++;  // Move to the next character in the input string.
    }

    // After the loop, ensure the last part is properly terminated and counted.
    if (j > 0) {
        parts[*count][j] = '\0';
        (*count)++;
    }
}

/**
 * Prints each part from the parts array.
 *
 * @param parts The 2D array containing the parts.
 * @param count The number of parts.
 */
void print_parts(char parts[MAX_PARTS][128], int count){
	for(int i = 0; i< count; i++){
		boot_print(parts[i]);	// Print the part
		boot_print("\n");	// Print a newline after each part
	}
}

int look_offset = 0;
int look_for_file(iso_9660_directory_entry_t *dir_entry, int count, int max_count, char parts[MAX_PARTS][128])
{

	unsigned int extent_start = dir_entry->extent_start_LSB;
	unsigned int extent_length = dir_entry->extent_length_LSB;

	
	/*
	boot_print("[Traversing] extent_start: ");
	boot_print_hex(extent_start);
	boot_print("\n");

	boot_print("[Traversing] extent_length: ");
	boot_print_hex(extent_length);
	boot_print("\n");
	*/


	// Read the directory entries
	// A special global variable nestedOffset is used to read the subdirectory
	// entry into to offset location, so that it would not overwrite the parent's
	// entry
	for (int itr = 0; itr < extent_length; itr += 2048)
	{
		ata_device_read_sector_atapi(device, extent_start + itr / 2048, dir_entries + look_offset + itr);

	}


	unsigned int offset = 0;

	while (offset < extent_length)
	{
		iso_9660_directory_entry_t *dir = (iso_9660_directory_entry_t *)(dir_entries + offset + look_offset);
		if (dir->length == 0)
		{
			//boot_print("end\n");

			// In case of end of the current directory entries, need to
			// decrement the nestedOffset by Block size, so when get back
			// previous (parent) directory read location.
			look_offset -= BLOCK_SIZE;
			// means we didnt found the file/directory
			return 0;	// Failure
		}
		if (!(dir->flags & FLAG_HIDDEN))
		{
			if (dir->name_len > 0 && dir->name_len < 256)
			{
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

				if (!strcmp(file_name, parts[count]))
				{
					if (count == max_count-1)
					{
					// reached the last part of the location
						// Reset (clear) the searched_file_dir_entry
						memsetb(searched_file_dir_entry, 2048, 0xA5);
						
						// Copy the directory entry at
						// searched_file_dir_entry = 0x40000
						memcpyb(searched_file_dir_entry, dir, sizeof(iso_9660_directory_entry_t));
						return 1;	// Success, return true
					}
					else
					{

						// If the entry is a directory, recurse into it
						if (dir->flags & FLAG_DIRECTORY)
						{

							/*char new_path[256];
							char file_name[dir->name_len + 1];
							memcpyb(file_name, dir->name, dir->name_len);
							file_name[dir->name_len] = 0;
							char *s = strchr(file_name, ';');
							if (s)
							{
								*s = '\0';
							}*/

							// Since we got to call the same function to
							// read the nested directories, it might
							// overwrite the current directory read
							// location, so nestedOffset is used,
							// it increments the read location by
							// block size thus, not overwriting the
							// current directory read location.
							look_offset += BLOCK_SIZE;
							// Increment count to process next section of location Path.
							count++;
							return look_for_file(dir, count, max_count, parts);
						}
					}
				}
			}
		}
		offset += dir->length;
	}
}


int navigate_to_file_directory_entry(char * name) {

	boot_print("[navigate_to_file_directory_entry]...\n");

//name = "KERNEL/AB.TXT";
// name = "KERNEL/KERNEL.ELF";
// 0 = KERNEL
// 1 = KERNEL.ELF
// 2 = '\0'
	char parts[MAX_PARTS][128];
	int count = 0;
	split_string(name, parts, &count);

// Print the parts of the Location string
	print_parts(parts, count);

// scan for the file with the location string
	int start_index = 0;
	int status = look_for_file(dir_entry, start_index, count, parts);

	/* boot_print("The Kernel = ");
	boot_print_hex(searched_file_dir_entry->extent_start_LSB);
	boot_print("\n");
	*/
	return status;
}

