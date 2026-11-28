# Makefile for src/boot/stage1

.PHONY: all clean

# Ensure ROOT_DIR is set, defaulting to one level up if not provided
ROOT_DIR ?= $(abspath ..)

# Include the configuration file
include $(ROOT_DIR)/config.mk

# Define Build Directory
BUILD_DIR = $(ROOT_DIR)/build
STAGE1_DIR = $(BUILD_DIR)/stage1

SRC_DIR = .
SRC = $(wildcard $(SRC_DIR)/*.asm)  # Finds all .asm files in the current directory
OBJ = $(patsubst $(SRC_DIR)/%.asm, $(STAGE1_DIR)/%.o, $(SRC))
BIN = $(patsubst $(SRC_DIR)/%.asm, $(STAGE1_DIR)/%.bin, $(SRC))


# Target to build all binaries
all: $(BIN)

# Rule to create the stage1 directory if it doesn't exist
$(STAGE1_DIR):
	@echo "$(CYAN)Creating stage1 directory...$(RESET)"
	mkdir -p $(STAGE1_DIR)

# Each object depends on the files it includes, not only on its source:
# nasm -MD writes them, as it builds, to a .d file beside the
# object, read back below (-MP: a phony target per included file, so a
# file removed later does not stop the build). Without them an edited
# .inc was not assembled again: the night tree's k130-any-size linked a
# stage2.o made before datastructure.inc said `global BootHeader`
# (TheTaaJ journal entry 148).

# Rule to build each binary
$(STAGE1_DIR)/%.bin: $(SRC_DIR)/%.asm | $(STAGE1_DIR) 
	@echo "$(GREEN)Assembling $< to $@...$(RESET)"
	$(ASM) $(STAGE1_ASFLAGS) -I $(ROOT_DIR)/$(BOOT_STAGE1_INCLUDE) \
            -Lm -Ls -l $(STAGE1_DIR)/$(notdir $(<:.asm=.lst)) \
            -dARCH=$(ARCH) \
            -MD $(@:.bin=.d) -MP \
            -I $(ROOT_DIR)/$(BOOT_COMMON_INCLUDE) $< -o $@

-include $(BIN:.bin=.d)

# Target to clean the output
clean:
	# TODO: Clean stage1 directory and other outputs
	@echo "$(YELLOW)Cleaning up the build directory...$(RESET)"
	rm -rf $(STAGE1_DIR)

