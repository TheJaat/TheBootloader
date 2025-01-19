# Compiler and tools
ASM = nasm
LD = ld

# Flags
ASFLAGS = -f bin
LDFLAGS = -m elf_i386

# Header includes
BOOT_STAGE1_INCLUDE = headers/boot/

# Common Directories
OUTPUT_DIR = ../../build
