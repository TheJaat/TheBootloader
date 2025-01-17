# Makefile for src/boot

ASM = nasm
ASFLAGS = -f bin
OUTPUT_DIR = ../../build
TARGET = $(OUTPUT_DIR)/stage1.bin

SRC = stage1.asm

.PHONY: all clean

# Target to build the bootloader binary
all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p $(OUTPUT_DIR)
	$(ASM) $(ASFLAGS) $(SRC) -o $(TARGET)

# Target to clean the output
clean:
	@rm -f $(TARGET)
	@rmdir --ignore-fail-on-non-empty $(OUTPUT_DIR)

