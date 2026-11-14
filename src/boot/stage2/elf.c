#include <elf.h>
#include <print.h>
#include <mem.h>
#include <constants.h>


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

/* ---- 64-bit kernels ------------------------------------------------ */

#define ELFCLASS64   2

/* Where the long-mode page tables map the kernel window: virtual
 * KERNEL_WINDOW + x is physical x, for x below KERNEL_WINDOW_SIZE. Must
 * match longmode.c, and the TaajBoot contract in taajboot.h. */
#define KERNEL_WINDOW      0xFFFFFFFF80000000ULL
#define KERNEL_WINDOW_SIZE 0x40000000ULL        /* 1 GiB */

/* load_ramdisk() runs after this and writes to RAMDISK_LOAD_ADDRESS, so
 * a kernel reaching that far would be overwritten by its own ramdisk.
 * The 32-bit kernel predates this check; a 64-bit one is held to it. */
#define LOAD_MAX_ADDRESS   RAMDISK_LOAD_ADDRESS

/* Set by load_elf64, read by stage2.asm and longmode.c. */
unsigned int       g_kernelIs64;
unsigned long long g_kernelEntry64;
unsigned long long g_kernelVirtual;     /* virtual address of g_kernelAddress */

static int load_elf64(void)
{
	Elf64_Header *header = (Elf64_Header *)KERNEL_ELF_READ;
	unsigned int  FileSize = (unsigned int)g_kernelSize;
	unsigned long long Lowest = ~0ULL, Highest = 0, LowestVirtual = 0;
	int Loaded = 0, EntryFound = 0;

	boot_print("[load_elf64], Loading an ELF64 file.\n");

	if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
		boot_print("[load_elf64], Not little-endian (EI_DATA).\n");
		return 0;
	}
	if (header->e_machine != EM_X86_64) {
		boot_print("[load_elf64], Not an x86_64 ELF (e_machine).\n");
		return 0;
	}
	if (header->e_type != ET_EXEC) {
		boot_print("[load_elf64], Not an executable ELF (e_type).\n");
		return 0;
	}
	if (header->e_phoff == 0 || header->e_phnum == 0
		|| header->e_phentsize < sizeof(Elf64_Phdr)) {
		boot_print("[load_elf64], Bad program header table.\n");
		return 0;
	}
	if (FileSize != 0 && header->e_phoff
			+ (unsigned long long)header->e_phnum * header->e_phentsize > FileSize) {
		boot_print("[load_elf64], Program headers run past end of file.\n");
		return 0;
	}

	for (int i = 0; i < header->e_phnum; i++) {
		Elf64_Phdr *phdr = (Elf64_Phdr *)((unsigned char *)header
			+ (unsigned int)header->e_phoff + i * header->e_phentsize);
		unsigned long long Dest = phdr->p_paddr;
		unsigned long long End  = Dest + phdr->p_memsz;

		if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
			continue;
		}

		/* Physical placement: above the loader, below the ramdisk. Both
		 * bounds are far below 4 GiB, so every value below fits in the
		 * 32-bit pointers this code runs with. */
		if (End < Dest || Dest < LOAD_MIN_ADDRESS || End > LOAD_MAX_ADDRESS) {
			boot_print("[load_elf64], Segment outside 1 MiB .. ramdisk, refusing.\n");
			return 0;
		}

		/* The segment has to be reachable through one of the two
		 * mappings longmode.c builds. Checking here turns a mislinked
		 * kernel into a message instead of a silent triple fault. */
		if (phdr->p_vaddr != Dest
			&& !(phdr->p_vaddr == Dest + KERNEL_WINDOW && End <= KERNEL_WINDOW_SIZE)) {
			boot_print("[load_elf64], Segment is neither identity- nor window-mapped.\n");
			return 0;
		}

		if (phdr->p_filesz > phdr->p_memsz
			|| (FileSize != 0 && phdr->p_offset + phdr->p_filesz > FileSize)) {
			boot_print("[load_elf64], Segment runs past end of file.\n");
			return 0;
		}

		memcpyb((void *)(unsigned int)Dest,
			(unsigned char *)KERNEL_ELF_READ + (unsigned int)phdr->p_offset,
			(unsigned int)phdr->p_filesz);
		memsetb((void *)(unsigned int)(Dest + phdr->p_filesz), 0,
			(int)(phdr->p_memsz - phdr->p_filesz));

		if (Dest < Lowest) {
			Lowest = Dest;
			LowestVirtual = phdr->p_vaddr;
		}
		if (End > Highest) {
			Highest = End;
		}
		/* e_entry is virtual, so it is checked against virtual ranges. */
		if (header->e_entry >= phdr->p_vaddr
			&& header->e_entry < phdr->p_vaddr + phdr->p_memsz) {
			EntryFound = 1;
		}
		Loaded++;
	}

	if (Loaded == 0) {
		boot_print("[load_elf64], No loadable segments found.\n");
		return 0;
	}
	if (!EntryFound) {
		boot_print("[load_elf64], e_entry is outside the loaded image.\n");
		return 0;
	}

	g_kernelAddress = (unsigned int)Lowest;
	g_kernelEnd     = (unsigned int)Highest;
	g_kernelVirtual = LowestVirtual;
	g_kernelEntry64 = header->e_entry;
	g_kernelIs64    = 1;

	boot_print("[load_elf64], Loading Elf64 done.\n");
	return 1;
}

/* load_elf
 * Loads whatever kernel load_kernel() staged: ELF32 boots the 32-bit
 * kernel exactly as before, ELF64 sets g_kernelIs64 so stage2 takes the
 * long-mode path. Returns 1 on success, 0 on failure. */
int load_elf(void)
{
	unsigned char *ident = (unsigned char *)KERNEL_ELF_READ;

	g_kernelIs64 = 0;

	if (ident[0] != ELFMAG0 || ident[1] != ELFMAG1
		|| ident[2] != ELFMAG2 || ident[3] != ELFMAG3) {
		boot_print("[load_elf], Not an ELF file.\n");
		return 0;
	}
	if (ident[EI_CLASS] == ELFCLASS64) {
		return load_elf64();
	}
	return load_elf32();
}