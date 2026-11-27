#include <mem.h>

/*typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
*/


#define HEAP_SIZE 0x10000 // 64 KB
static unsigned char heap[HEAP_SIZE];
static unsigned int heap_offset = 0;

void* stage2_malloc(unsigned int size) {
	// Align size to 4 bytes for better alignment.
	size = (size + 3) & ~3;
	if(heap_offset + size > HEAP_SIZE) {
		// Out of memory.
		return (void*)0;
	}
	void* ptr = &heap[heap_offset];
	heap_offset += size;
	return ptr;
}

/* The copying and filling everything in stage 2 comes down to: the
 * kernel's 1.7 MB from the BIOS buffer into place, then again from there
 * to its segments, and its .bss zeroed. They were C loops of one byte per
 * turn, compiled without optimisation: emulated, ten of an emulated
 * boot's twelve seconds before the kernel (K23, journal entry NNN). Now
 * the x86's own block instructions: `rep movsd` copies four bytes a step
 * and `rep stosd` fills four (`movsb`/`stosb` for what is left) - one
 * instruction for the whole block, which a CPU runs at memory speed and
 * an emulator runs as one operation, not a loop of translated steps.
 *
 * Forward, as before: a copy to a lower address that overlaps its source
 * is still right; to a higher one it never was. A count of 0 or less does
 * nothing (the old `while (n--)` ran for ever on a negative one). */
static void copy_forward(void *dest, const void *src, unsigned int n) {
	unsigned int words = n / 4, bytes = n % 4;
	__asm__ volatile ("cld\n\trep movsl" : "+D"(dest), "+S"(src), "+c"(words) : : "memory");
	__asm__ volatile ("rep movsb" : "+D"(dest), "+S"(src), "+c"(bytes) : : "memory");
}

static void fill(void *dest, unsigned char value, unsigned int n) {
	unsigned int words = n / 4, bytes = n % 4;
	unsigned int pattern = value * 0x01010101u;
	__asm__ volatile ("cld\n\trep stosl" : "+D"(dest), "+c"(words) : "a"(pattern) : "memory");
	__asm__ volatile ("rep stosb" : "+D"(dest), "+c"(bytes) : "a"(pattern) : "memory");
}

void* memcpy(void *dest, const void *src, int n) {
	if (n > 0) {
		copy_forward(dest, src, (unsigned int)n);
	}
	return dest;
}

/* memcpyb: the same, with a size_t count. */
void *memcpyb(void *dest, const void *src, size_t n) {
	copy_forward(dest, src, n);
	return dest;
}

/* memsetb: `n` bytes of `dest` set to `c`. */
void *memsetb(void *dest, int c, int n) {
	if (n > 0) {
		fill(dest, (unsigned char)c, (unsigned int)n);
	}
	return dest;
}

int strcmp(const char * l, const char * r) {
	for (; *l == *r && *l; l++, r++);
	return *(unsigned char *)l - *(unsigned char *)r;
}

char * strchr(const char * s, int c) {
	while (*s) {
		if (*s == c) {
			return (char *)s;
		}
		s++;
	}
	return 0;
}

char *strncpy(char *dest, const char *src, unsigned int n) {
	unsigned int i = 0;

	// Copy characters from src to dest until either n characters are copied
	// or the end of src is reached.
	while(i < n && src[i] != '\0') {
		dest[i] = src[i];
		i++;
	}

	// If we haven't copied n characters, pad the reaminder fo dest with '\0'
	while(i < n) {
		dest[i] = '\0';
		i++;
	}
	return dest;
}

