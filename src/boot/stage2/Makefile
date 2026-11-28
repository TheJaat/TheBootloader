# Makefile for src/boot/stage2

.PHONY: all clean

# Ensure ROOT_DIR is set, defaulting to one level up if not provided
ROOT_DIR ?= $(abspath ..)

# Include the configuration file
include $(ROOT_DIR)/config.mk

# Define Build Directory
BUILD_DIR = $(ROOT_DIR)/build
STAGE2_DIR = $(BUILD_DIR)/stage2

SRC_DIR = .
# stage2.asm MUST be linked first: stage1 jumps to the very first byte of
# STAGE2.BIN, and stage2.asm's real-mode labels must stay below 0xFFFF.
# Every other .asm file (longmode.asm) follows it, then the C objects.
ASM_SRC = $(SRC_DIR)/stage2.asm $(filter-out $(SRC_DIR)/stage2.asm, $(wildcard $(SRC_DIR)/*.asm))
C_SRC = $(wildcard $(SRC_DIR)/*.c)       # Finds all .c files in the current directory

OBJ = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.o, $(ASM_SRC)) \
      $(patsubst $(SRC_DIR)/%.c, $(STAGE2_DIR)/%.elf, $(C_SRC))

BIN = $(STAGE2_DIR)/stage2.bin           # Final binary for Stage 2

# Target to build all binaries
all: $(BIN)

# Rule to create the stage2 directory if it doesn't exist
$(STAGE2_DIR):
	@echo "$(CYAN)Creating stage2 directory...$(RESET)"
	mkdir -p $(STAGE2_DIR)

# Each object depends on the files it includes, not only on its source:
# nasm -MD and gcc -MMD write them, as it builds, to a .d file beside the
# object, read back below (-MP: a phony target per included file, so a
# file removed later does not stop the build). Without them an edited
# .inc was not assembled again: the night tree's k130-any-size linked a
# stage2.o made before datastructure.inc said `global BootHeader`
# (TheTaaJ journal entry 148).

# Rule to assemble .asm files to .o
$(STAGE2_DIR)/%.o: $(SRC_DIR)/%.asm | $(STAGE2_DIR) 
	@echo "$(GREEN)Assembling $< to $@...$(RESET)"
	$(ASM) $(STAGE2_ASFLAGS) -MD $(@:.o=.d) -MP -I $(ROOT_DIR)/$(BOOT_COMMON_INCLUDE) -I $(ROOT_DIR)/$(BOOT_STAGE2_ASM_INCLUDE) $< -o $@

# Rule to compile .c files to .o
$(STAGE2_DIR)/%.elf: $(SRC_DIR)/%.c | $(STAGE2_DIR)
	@echo "$(BLUE)Compiling $< to $@...$(RESET)"
	$(CC) $(GCC_FLAGS) -MMD -MP -I $(ROOT_DIR)/$(BOOT_STAGE2_C_INCLUDE) -I $(ROOT_DIR)/$(BOOT_STAGE2_C_STD_INCLUDE) -I $(ROOT_DIR)/$(HEADERS) -c $< -o $@
	@# revision.h out: every build bumps it (increase-build), and
	@# print.c and stage2.bin would be built again each time.
	@# The path only, not its line: the line before may end in "\".
	@sed -i -e 's| [^ ]*revision\.h||' -e '/^[^ ]*revision\.h:/d' $(@:.elf=.d)

# Rule to link all object files into the final binary
$(BIN): $(OBJ)
	@echo "$(MAGENTA)Linking $@...$(RESET)"
	ld -m elf_i386 -T stage2.ld --oformat binary -o $@ $^

-include $(patsubst %.o,%.d,$(filter %.o,$(OBJ))) $(patsubst %.elf,%.d,$(filter %.elf,$(OBJ)))

# Target to clean the output
clean:
	# Clean stage2 directory and other outputs
	@echo -e "$(YELLOW)Cleaning up the build directory...$(RESET)"
	rm -rf $(STAGE2_DIR)

