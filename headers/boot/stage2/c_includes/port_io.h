#ifndef PORT_IO_H
#define PORT_IO_H

// Declaration port functions
extern unsigned char inb(unsigned short port);
extern unsigned short inw(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void outw(unsigned short port, unsigned short data);
extern void inwm(unsigned short port, unsigned char * data, unsigned long size);

#endif // PORT_IO_H
