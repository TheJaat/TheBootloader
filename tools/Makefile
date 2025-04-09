# Makefile for tools/

.PHONY: all clean revision

# Ensure ROOT_DIR is set, defaulting to one level up if not provided
ROOT_DIR ?= $(abspath ..)

# Include the configuration file
include $(ROOT_DIR)/config.mk

# Define Build Directory
Build_DIR = $(ROOT_DIR)/build

# Subdirectories
SUBDIRS = revision

# Target to build everything
all: revision

# Build the revision tool
revision:
	@echo "$(GREEN)Building revision tool...$(RESET)"
	$(MAKE) -C revision ROOT_DIR=$(ROOT_DIR)

# Target to clean everything
clean:
	@echo "$(YELLOW)Cleaning tools directory...$(RESET)"
	rm -rf $(BUILD_DIR)
