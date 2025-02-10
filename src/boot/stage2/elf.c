#include <elf.h>
#include <print.h>
#include <mem.h>


// Define the memory start location where the ELF file is loaded
#define KERNEL_ELF_READ 0x300000
#define KERNEL_DEST_START 0x1000000

// This global variable stores the address where kernel is loaded
// It is used in stage2.asm file as extern
unsigned int g_kernelAddress;

int load_elf32()
{
	boot_print("[load_elf32], Loading an ELF32 file.\n");

	Elf32_Header *header = (Elf32_Header *)KERNEL_ELF_READ;
	if (header->e_ident[0] != ELFMAG0 ||
		header->e_ident[1] != ELFMAG1 ||
		header->e_ident[2] != ELFMAG2 ||
		header->e_ident[3] != ELFMAG3)
	{
		// ERROR not an ELF File
		boot_print("[load_elf32], Not an ELF file.\n");
		return 0;		// return false
	}

	// uintptr_t entry = (uintptr_t)header->e_entry;

	// Iterate through program headers
	for (int i = 0; i < header->e_phnum; i++)
	{
		// Calculate the address of the program header
		Elf32_Phdr *phdr = (Elf32_Phdr *)((unsigned char *)header + header->e_phoff + i * header->e_phentsize);

		// If the segment is loadable
		if (phdr->p_type == PT_LOAD)
		{
			boot_print("[load_elf32], Loading a Phdr.\n");
			// Calculate the source and destination addresses
			unsigned char *src = (unsigned char *)KERNEL_ELF_READ + phdr->p_offset;
			// unsigned char *dst = (unsigned char *)KERNEL_DEST_START + phdr->p_vaddr;
			unsigned char *dst = (unsigned char *)phdr->p_vaddr;

			// store the kernel elf destination address
			g_kernelAddress = phdr->p_vaddr;

			// Copy the segment to the virtual address
			memcpyb(dst, src, phdr->p_filesz);

			// Zero out the rest of the segment if p_memsz > p_filesz
			memsetb(dst + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
		}
	}
	boot_print("[load_elf32], Loading Elf32 done.\n");

	// In Case just jump to the loaded position of the file implicitly,
	
	void (*entry_point)(void) = (void(*)(void))header->e_entry;
		entry_point();

	return 1;	// Return true
}

