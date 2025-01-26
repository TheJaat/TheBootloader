#include <port_io.h>

// Definition of functions
inline unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

inline unsigned short inw(unsigned short port) {
    unsigned short result;
    __asm__ __volatile__("inw %1, %0" : "=a"(result) : "dN"(port));
    return result;
}

inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "dN"(port));
}

inline void outw(unsigned short port, unsigned short data) {
    __asm__ __volatile__("outw %0, %1" : : "a"(data), "dN"(port));
}

inline void inwm(unsigned short port, unsigned char * data, unsigned long size) {
	asm volatile ("rep insw" : "+D" (data), "+c" (size) : "d" (port) : "memory");
}


