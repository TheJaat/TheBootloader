#include <mem.h>

/*typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
*/
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

