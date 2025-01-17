# Makefile for src/

.PHONY: all clean

# Subdirectory
SUBDIRS = boot

# Target to build everything
all:
	@for dir in $(SUBDIRS); do\
		$(MAKE) -C $$dir || exit 1; \
	done

# Target to clean everything
clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || exit 1; \
	done
