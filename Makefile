# Root Makefile

.PHONY: all run clean

# Subdirectories
SUBDIRS = src

# Define ROOT_DIR before including config.mk
ROOT_DIR := $(abspath .)

include $(ROOT_DIR)/config.mk

BUILD_DIR = build

STAGE1_DIR = $(BUILD_DIR)/stage1
STAGE1_BIN = $(STAGE1_DIR)/stage1.bin
STAGE2_DIR = $(BUILD_DIR)/stage2
STAGE2_BIN = $(STAGE2_DIR)/stage2.bin

DISK_IMG := $(BUILD_DIR)/disk.img
DISK_SIZE := 1440  # Disk size in KB

# Target to build everything
all:
	# Create the build directory
	@mkdir -p build

	@for dir in $(SUBDIRS); do \
		echo "$(MAGENTA)Entering Directory $$dir...$(RESET)";\
		$(MAKE) -C $$dir ROOT_DIR=$(ROOT_DIR) || { \
			echo "$(RED)Error: Make failed in $$dir!$(RESET)"; \
			exit 1; \
		} \
	done

$(DISK_IMG):
	@echo "Creating disk image at $(DISK_IMG)"
	# Create an empty disk image
	dd if=/dev/zero of=$(DISK_IMG) bs=1k count=$(DISK_SIZE)
	# Copy the stage1 binary to the first sector of the disk
	dd if=$(STAGE1_BIN) of=$(DISK_IMG) bs=512 count=1 conv=notrunc
	# Copy the stage2 binary to the disk starting at sector 2
	dd if=$(STAGE2_BIN) of=$(DISK_IMG) bs=512 seek=1 conv=notrunc

run: $(DISK_IMG)
	qemu-system-x86_64 -drive format=raw,file=$(DISK_IMG)

# Target to clean everything
clean:
	# $(MAKE) -C src clean
	@rm -rf $(BUILD_DIR)
