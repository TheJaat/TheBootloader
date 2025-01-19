# Makefile for src/boot

.PHONY: all clean

# Ensure ROOT_DIR is set, defaulting to one level up if not provided
ROOT_DIR ?= $(abspath ..)

# Include the configuration file
include $(ROOT_DIR)/config.mk

# Define Build Directory
Build_DIR = $(ROOT_DIR)/build

# Subdirectory
SUBDIRS = stage1

# Target to build everything
all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir ROOT_DIR=$(ROOT_DIR) || exit 1; \
	done

# Target to clean everything
clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done
