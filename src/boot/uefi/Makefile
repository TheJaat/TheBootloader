# Makefile for src/boot/uefi - TheTaaJBoot for UEFI: BOOTX64.EFI for
# the PC, BOOTAA64.EFI for AArch64 (`make aarch64`).
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

# The AArch64 loader: the same compiler, told another CPU. It is one
# file (aarch64.c) and needs no flags of the PC's (there is no red zone
# in the Arm calling convention to switch off). -mgeneral-regs-only: no
# floating point registers, which the firmware need not have enabled.
A64_TARGET = $(OUT_DIR)/BOOTAA64.EFI
A64_CFLAGS = -target aarch64-unknown-windows \
             -std=c11 -O2 -Wall -Wextra -Werror \
             -ffreestanding -fno-stack-protector -fno-stack-check \
             -mgeneral-regs-only -fshort-wchar

.PHONY: all aarch64 clean

all: $(TARGET)

aarch64: $(A64_TARGET)

$(OUT_DIR)/aarch64.obj: aarch64.c efi.h common.h
	@mkdir -p $(OUT_DIR)
	$(CC) $(A64_CFLAGS) -c $< -o $@

$(A64_TARGET): $(OUT_DIR)/aarch64.obj
	$(LINK) $(LDFLAGS) -out:$@ $<

$(OUT_DIR)/%.obj: %.c efi.h common.h elf64.h $(ROOT_DIR)/headers/boot/taajboot.h
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(LINK) $(LDFLAGS) -out:$@ $(OBJECTS)

clean:
	rm -rf $(OUT_DIR)
