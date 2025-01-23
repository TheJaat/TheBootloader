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
SRC = $(wildcard $(SRC_DIR)/*.asm)  # Finds all .asm files in the current directory
OBJ = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.o, $(SRC))
BIN = $(patsubst $(SRC_DIR)/%.asm, $(STAGE2_DIR)/%.bin, $(SRC))


# Target to build all binaries
all: $(BIN)

# Rule to create the stage2 directory if it doesn't exist
$(STAGE2_DIR):
	@echo "$(CYAN)Creating stage2 directory...$(RESET)"
	mkdir -p $(STAGE2_DIR)

# Rule to build each binary
$(STAGE2_DIR)/%.bin: $(SRC_DIR)/%.asm | $(STAGE2_DIR) 
	@echo "$(GREEN)Assembling $< to $@...$(RESET)"
	$(ASM) $(ASFLAGS) -I $(ROOT_DIR)/$(BOOT_STAGE1_INCLUDE) $< -o $@

# Target to clean the output
clean:
	# Clean stage2 directory and other outputs
	@echo -e "$(YELLOW)Cleaning up the build directory...$(RESET)"
	rm -rf $(STAGE2_DIR)

