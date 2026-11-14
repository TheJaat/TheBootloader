#ifndef __ELF64_H__
#define __ELF64_H__

/* elf64.h - the two ELF64 structures a loader needs: the file header,
 * which says where the program headers are and where execution starts,
 * and the program headers, which say what to copy where. Section
 * headers are for linkers and debuggers; a loader ignores them. */

#include <stdint.h>

#define ELF_MAGIC       0x464C457FU     /* "\x7FELF" read as a little-endian u32 */
#define ELF_CLASS_64    2               /* e_ident[4] */
#define ELF_DATA_LSB    1               /* e_ident[5]: little-endian */
#define ELF_TYPE_EXEC   2
#define ELF_MACHINE_X86_64 62

#define PT_LOAD         1

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;           /* virtual address of the first instruction */
    uint64_t e_phoff;           /* file offset of the program headers */
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;       /* size of one program header */
    uint16_t e_phnum;           /* how many */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;          /* where the bytes are in the file */
    uint64_t p_vaddr;           /* where the program expects them */
    uint64_t p_paddr;           /* where to put them in physical memory */
    uint64_t p_filesz;          /* bytes to copy */
    uint64_t p_memsz;           /* bytes to occupy; the rest is zeroed (.bss) */
    uint64_t p_align;
} Elf64_Phdr;

typedef char Elf64EhdrSizeCheck[(sizeof(Elf64_Ehdr) == 64) ? 1 : -1];
typedef char Elf64PhdrSizeCheck[(sizeof(Elf64_Phdr) == 56) ? 1 : -1];

#endif /* __ELF64_H__ */
