# Makefile for src/boot/stage1

.PHONY: all clean

# Ensure ROOT_DIR is set, defaulting to one level up if not provided
ROOT_DIR ?= $(abspath ..)

# Include the configuration file
include $(ROOT_DIR)/config.mk

# Define Build Directory
BUILD_DIR = $(ROOT_DIR)/build

SRC_DIR = .
SRC = $(wildcard $(SRC_DIR)/*.asm)  # Finds all .asm files in the current directory
OBJ = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(SRC))
BIN = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.bin, $(SRC))


# Target to build all binaries
all: $(BIN)

# Rule to build each binary
$(BUILD_DIR)/%.bin: $(SRC_DIR)/%.asm
	echo "Assembling $< to $@"
	$(ASM) $(ASFLAGS) -I $(ROOT_DIR)/$(BOOT_STAGE1_INCLUDE) $< -o $@

# Target to clean the output
clean:
	# TODO later

