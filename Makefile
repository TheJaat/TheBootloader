# Root Makefile

.PHONY: all run clean

# Subdirectories
SUBDIRS = src

# Define ROOT_DIR before including config.mk
ROOT_DIR := $(abspath .)

include $(ROOT_DIR)/config.mk

BUILD_DIR = build


# Target to build everything
all:
	# Create the build directory
	@mkdir -p build

	@for dir in $(SUBDIRS); do \
		$(MAKE) -C  $$dir ROOT_DIR=$(ROOT_DIR); \
	done


run:
	qemu-system-x86_64 -drive format=raw,file=build/stage1.bin

# Target to clean everything
clean:
	# $(MAKE) -C src clean
	@rm -rf $(BUILD_DIR)
