# Host tests for stage 2's C code (K129): the ISO 9660 reader, the ELF
# loader, the long-mode page tables and boot info.
#
#   make -C tests                       build and run
#   make -C tests ISO=... KERNEL=...    with a real ISO and kernel too
#
# Built with stage 2's own compiler and flags, from stage 2's own files:
# the code tested is the code that boots (harness.h says how a 32-bit
# freestanding program runs it at stage 2's addresses on Linux).

ROOT_DIR ?= $(abspath ..)
include $(ROOT_DIR)/config.mk
# config.mk finds the cross compiler relative to where make runs - right
# from bootloader/, not from here: from ROOT_DIR instead.
export PATH := $(abspath $(ROOT_DIR)/../toolchain/i686)/bin:$(PATH)

BUILD = $(ROOT_DIR)/build/tests
STAGE2 = $(ROOT_DIR)/src/boot/stage2
INCLUDES = -I $(ROOT_DIR)/$(BOOT_STAGE2_C_INCLUDE) -I $(ROOT_DIR)/$(BOOT_STAGE2_C_STD_INCLUDE) -I $(ROOT_DIR)/$(HEADERS)

# Stage 2's files exactly as stage 2 compiles them (its Makefile's flags).
UNDER_TEST = ISO9660FS mem elf bios_disk
# The tests themselves: warnings on, and the same target and freedom.
TEST_FLAGS = $(GCC_FLAGS) -Wall -Wno-pointer-sign -Wno-discarded-qualifiers -O1 -g

OBJ = $(addprefix $(BUILD)/,$(addsuffix .o,$(UNDER_TEST))) \
      $(BUILD)/harness.o $(BUILD)/iso_test.o $(BUILD)/elf_test.o $(BUILD)/longmode_test.o
PROGRAM = $(BUILD)/stage2-tests

.PHONY: all run clean
all: run

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: $(STAGE2)/%.c | $(BUILD)
	$(CC) $(GCC_FLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.c harness.h $(STAGE2)/longmode.c | $(BUILD)
	$(CC) $(TEST_FLAGS) $(INCLUDES) -c $< -o $@

# Linked far above stage 2's addresses (Linux's usual 0x08048000), so
# that the low 16 MiB are free for the tests to map.
$(PROGRAM): $(OBJ)
	$(CC) -m32 -nostdlib -static -Wl,-Ttext-segment=0x08048000 -o $@ $^ -lgcc

run: $(PROGRAM)
	$(PROGRAM) $(ISO) $(KERNEL)

clean:
	rm -rf $(BUILD)
