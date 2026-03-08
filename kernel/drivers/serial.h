#ifndef SERIAL_H
#define SERIAL_H

#include "../core/kernel.h"

#define SERIAL_COM1     0x3F8
#define SERIAL_COM2     0x2F8

void serial_init(u16 port);
void serial_putchar(u16 port, char c);
void serial_puts(u16 port, const char *str);
void serial_put_hex(u16 port, u64 value);
char serial_getchar(u16 port);
int  serial_received(u16 port);

#endif