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


# Color variables
# Color variables
BLACK = \033[30m
RED = \033[31m
GREEN = \033[32m
YELLOW = \033[33m
BLUE = \033[34m
MAGENTA = \033[35m
CYAN = \033[36m
WHITE = \033[37m

LIGHT_BLUE = \033[38;5;45m
BRIGHT_YELLOW = \033[38;5;226m
LIGHT_CYAN = \033[38;5;51m
BOLD_BLUE = \033[1;34m
RESET = \033[0m

