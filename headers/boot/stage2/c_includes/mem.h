#ifndef __MEM_H__
#define __MEM_H__

#include <stdint.h>
#include <stddef.h>

extern void *memcpyb(void *dest, const void *src, size_t n);
extern void *memsetb(void *dest, int c, int n);

extern int strcmp(const char * l, const char * r);

extern char *strchr(const char * s, int c);

#endif /* __MEM_H__ */

