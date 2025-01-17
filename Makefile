# Root Makefile

.PHONY: all run clean


# Target to build everything
all:
	$(MAKE) -C  src

run:
	qemu-system-x86_64 -drive format=raw,file=build/stage1.bin

# Target to clean everything
clean:
	$(MAKE) -C src clean
