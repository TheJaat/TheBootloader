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
ASM_SRC = $(wildcard $(SRC_DIR)/*.asm)    # Finds all .asm files in the current directory
C_SRC = $(wildcard $(SRC_DIR)/*.c)       # Finds all .c files in the current directory
# SRC = $(wildcard $(SRC_DIR)/*.asm)  # Finds all .asm files in the current directory
# OBJ = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.o, $(SRC))
# BIN = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.bin, $(SRC))
OBJ = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.o, $(ASM_SRC)) \
      $(patsubst $(SRC_DIR)/%.c, $(STAGE2_DIR)/%.elf, $(C_SRC))

BIN = $(STAGE2_DIR)/stage2.bin           # Final binary for Stage 2

# Target to build all binaries
all: $(BIN)

# Rule to create the stage2 directory if it doesn't exist
$(STAGE2_DIR):
	@echo "$(CYAN)Creating stage2 directory...$(RESET)"
	mkdir -p $(STAGE2_DIR)

# Rule to assemble .asm files to .o
$(STAGE2_DIR)/%.o: $(SRC_DIR)/%.asm | $(STAGE2_DIR) 
	@echo "$(GREEN)Assembling $< to $@...$(RESET)"
	$(ASM) $(STAGE2_ASFLAGS) -I $(ROOT_DIR)/$(BOOT_COMMON_INCLUDE) -I $(ROOT_DIR)/$(BOOT_STAGE2_ASM_INCLUDE) $< -o $@

# Rule to compile .c files to .o
$(STAGE2_DIR)/%.elf: $(SRC_DIR)/%.c | $(STAGE2_DIR)
	@echo "$(BLUE)Compiling $< to $@...$(RESET)"
	$(CC) $(GCC_FLAGS) -I $(ROOT_DIR)/$(BOOT_STAGE2_C_INCLUDE) -I $(ROOT_DIR)/$(BOOT_STAGE2_C_STD_INCLUDE) -c $< -o $@

# Rule to link all object files into the final binary
$(BIN): $(OBJ)
	@echo "$(MAGENTA)Linking $@...$(RESET)"
	ld -m elf_i386 -T stage2.ld --oformat binary -o $@ $^

# Target to clean the output
clean:
	# Clean stage2 directory and other outputs
	@echo -e "$(YELLOW)Cleaning up the build directory...$(RESET)"
	rm -rf $(STAGE2_DIR)

