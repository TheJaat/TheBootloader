#include <elf.h>
#include <print.h>
#include <mem.h>


// Where load_kernel() staged the raw kernel.elf.
#define KERNEL_ELF_READ 0x300000

// The bootloader itself lives at 0xCC00 and its subsystem buffers run to
// 0x9FFFF. A kernel segment must not land on either, nor on the staging
// buffer we are copying *from*.
#define LOAD_MIN_ADDRESS  0x100000

// e_ident indices and values we care about (not in elf.h)
#define EI_CLASS     4
#define EI_DATA      5
#define EI_VERSION   6
#define ELFCLASS32   1
#define ELFDATA2LSB  1

// Size of kernel.elf on disk, set by load_kernel() in vfs.c. Used only to
// bounds-check the program headers against the buffer we actually read.
extern int g_kernelSize;

// Lowest load address of any PT_LOAD segment.
unsigned int g_kernelAddress;

// One past the highest load address + p_memsz of any PT_LOAD segment.
// This is the value the physical memory manager needs in order to reserve
// the kernel's frames. It is not the same as the size of kernel.elf on
// disk: the file includes the section header table and symbols, and
// excludes .bss.
unsigned int g_kernelEnd;

// The ELF entry point (e_entry). stage2 must jump here rather than to a
// hard-coded address: kernel.ld places .multiboot at 0x100000 and aligns
// .text to the next 4K boundary, so kernel_entry lives at 0x101000.
unsigned int g_kernelEntry;

int load_elf32()
{
	boot_print("[load_elf32], Loading an ELF32 file.\n");

	Elf32_Header *header = (Elf32_Header *)KERNEL_ELF_READ;
	unsigned int  FileSize = (unsigned int)g_kernelSize;

	if (header->e_ident[0] != ELFMAG0 ||
		header->e_ident[1] != ELFMAG1 ||
		header->e_ident[2] != ELFMAG2 ||
		header->e_ident[3] != ELFMAG3)
	{
		boot_print("[load_elf32], Not an ELF file.\n");
		return 0;
	}

	// Reject anything we are not actually able to load, rather than
	// parsing it as if it were a 32-bit little-endian x86 executable.
	if (header->e_ident[EI_CLASS] != ELFCLASS32) {
		boot_print("[load_elf32], Not a 32-bit ELF (EI_CLASS).\n");
		return 0;
	}
	if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
		boot_print("[load_elf32], Not little-endian (EI_DATA).\n");
		return 0;
	}
	if (header->e_machine != EM_386) {
		boot_print("[load_elf32], Not an i386 ELF (e_machine).\n");
		return 0;
	}
	// ET_DYN would need relocation processing, which we do not do.
	if (header->e_type != ET_EXEC) {
		boot_print("[load_elf32], Not an executable ELF (e_type).\n");
		return 0;
	}
	if (header->e_phoff == 0 || header->e_phnum == 0) {
		boot_print("[load_elf32], No program headers.\n");
		return 0;
	}
	// We index program headers with e_phentsize, so it has to be at least
	// as large as the struct we are about to dereference.
	if (header->e_phentsize < sizeof(Elf32_Phdr)) {
		boot_print("[load_elf32], Bad e_phentsize.\n");
		return 0;
	}
	if (FileSize != 0 &&
		(header->e_phoff + (unsigned int)header->e_phnum * header->e_phentsize)
			> FileSize) {
		boot_print("[load_elf32], Program headers run past end of file.\n");
		return 0;
	}

	unsigned int Lowest   = 0xFFFFFFFF;
	unsigned int Highest  = 0;
	int          Loaded   = 0;

	for (int i = 0; i < header->e_phnum; i++)
	{
		Elf32_Phdr *phdr = (Elf32_Phdr *)((unsigned char *)header
			+ header->e_phoff + i * header->e_phentsize);

		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		// Zero-size segments carry nothing and would only skew the span.
		if (phdr->p_memsz == 0) {
			continue;
		}

		// Load at the *physical* address. GNU ld sets p_paddr == p_vaddr
		// unless the linker script uses AT(), so this is identical to the
		// old p_vaddr behaviour today - but it is what keeps this loader
		// working if the kernel ever moves to a higher-half layout.
		unsigned int Dest = phdr->p_paddr;
		unsigned int End  = Dest + phdr->p_memsz;

		// Overflow, and anything that would land on the bootloader, its
		// buffers, or the staging copy we are reading from.
		if (End < Dest) {
			boot_print("[load_elf32], Segment address overflows.\n");
			return 0;
		}
		if (Dest < LOAD_MIN_ADDRESS) {
			boot_print("[load_elf32], Segment below 1mB, refusing.\n");
			return 0;
		}
		if (Dest < (KERNEL_ELF_READ + FileSize) && End > KERNEL_ELF_READ) {
			boot_print("[load_elf32], Segment overlaps the ELF staging buffer.\n");
			return 0;
		}

		// The bytes we are about to read must actually be inside the file.
		if (phdr->p_filesz > phdr->p_memsz) {
			boot_print("[load_elf32], p_filesz > p_memsz.\n");
			return 0;
		}
		if (FileSize != 0 && (phdr->p_offset + phdr->p_filesz) > FileSize) {
			boot_print("[load_elf32], Segment runs past end of file.\n");
			return 0;
		}

		boot_print("[load_elf32], Loading a Phdr.\n");

		unsigned char *src = (unsigned char *)KERNEL_ELF_READ + phdr->p_offset;
		unsigned char *dst = (unsigned char *)Dest;

		memcpyb(dst, src, phdr->p_filesz);
		memsetb(dst + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

		if (Dest < Lowest) {
			Lowest = Dest;
		}
		if (End > Highest) {
			Highest = End;
		}
		Loaded++;
	}

	if (Loaded == 0) {
		boot_print("[load_elf32], No loadable segments found.\n");
		return 0;
	}

	// The entry point must fall inside something we actually loaded.
	if (header->e_entry < Lowest || header->e_entry >= Highest) {
		boot_print("[load_elf32], e_entry is outside the loaded image.\n");
		return 0;
	}

	g_kernelAddress = Lowest;
	g_kernelEnd     = Highest;
	g_kernelEntry   = header->e_entry;

	boot_print("[load_elf32], Loading Elf32 done.\n");
	return 1;
}