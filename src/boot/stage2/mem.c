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

void* memcpy(void *dest, const void *src, int n) {

	unsigned char *d = (unsigned char*) dest;
	const unsigned char *s = (const unsigned char*) src;

	while(n--) {
		*d++ = *s++;
	}

	return dest;
}

/*
 * memcpy
 * Set `count` bytes to `val`.
 */
// Byte Version of Memcpy
void *memcpyb(void *dest, const void *src, size_t n) {

	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	// Copy bytes from src to dest
	while (n--) {
		*d++ = *s++;
	}
    
	// OR
	/*
	for(int i = 0; i< count; i++)
	{
		dest[i] = src[i];
	}
	*/

	return dest;
}


/*
 * memsetb
 * Set `count` bytes to `val`.
 */
void *memsetb(void *dest, int c, int n) {

	unsigned char *p = (unsigned char *)dest;

	while (n--) {
		*p++ = (unsigned char)c;
	}
	// OR
	/*
	for(int i = 0; i < n; i++){
		dest[i] = val;
	}
	*/
	
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

