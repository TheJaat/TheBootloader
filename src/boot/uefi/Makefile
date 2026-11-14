# Makefile for src/boot/uefi - TheTaaJBoot for UEFI, as BOOTX64.EFI.
#
# A UEFI application is a Windows-style PE/COFF executable, not ELF, so
# GCC's ELF toolchain cannot produce one directly. Clang can, by
# targeting Windows: same C, PE output, and the Microsoft calling
# convention UEFI uses. lld-link (LLVM's linker in its Windows mode)
# writes the .efi file.
#
# Standalone (not part of the stage1/stage2 build), so the BIOS loader
# still builds on a machine without Clang.

ROOT_DIR ?= $(abspath ../../..)
OUT_DIR   = $(ROOT_DIR)/build/uefi
TARGET    = $(OUT_DIR)/BOOTX64.EFI

CC   = clang
LINK = lld-link

CFLAGS = -target x86_64-unknown-windows \
         -std=c11 -O2 -Wall -Wextra -Werror \
         -ffreestanding -fno-stack-protector -fno-stack-check \
         -mno-red-zone -fshort-wchar \
         -I$(ROOT_DIR)/headers/boot

# efi_application: the subsystem the firmware checks for. No default
# libraries: there is no C library under UEFI - main.c brings its own
# memcpy/memset.
LDFLAGS = -subsystem:efi_application -entry:efi_main -nodefaultlib -dll:no

SOURCES = main.c
OBJECTS = $(SOURCES:%.c=$(OUT_DIR)/%.obj)

.PHONY: all clean

all: $(TARGET)

$(OUT_DIR)/%.obj: %.c efi.h elf64.h $(ROOT_DIR)/headers/boot/taajboot.h
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(LINK) $(LDFLAGS) -out:$@ $(OBJECTS)

clean:
	rm -rf $(OUT_DIR)
