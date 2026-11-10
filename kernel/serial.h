/* The first serial port (COM1): the kernel console. */
#ifndef XM3_SERIAL_H
#define XM3_SERIAL_H

void serial_init(void);
void serial_putc(char c);

#endif
