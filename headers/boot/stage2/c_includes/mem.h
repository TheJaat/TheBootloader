#ifndef __MEM_H__
#define __MEM_H__

#include <stdint.h>
#include <stddef.h>

void* stage2_malloc(unsigned int size);

extern void *memcpyb(void *dest, const void *src, size_t n);
extern void *memsetb(void *dest, int c, int n);

extern int strcmp(const char * l, const char * r);

extern char *strchr(const char * s, int c);

char *strncpy(char *dest, const char *src, unsigned int n);

#endif /* __MEM_H__ */

