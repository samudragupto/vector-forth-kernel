/*=============================================================================
 * Serial (UART) Driver implementation
 *=============================================================================*/

#include "serial.h"
#include "../utils/stdlib.h"

void serial_init(u16 port) {
    outb(port + 1, 0x00);   /* Disable all interrupts */
    outb(port + 3, 0x80);   /* Enable DLAB (set baud rate divisor) */
    outb(port + 0, 0x03);   /* Set divisor to 3 (38400 baud) lo byte */
    outb(port + 1, 0x00);   /*                                  hi byte */
    outb(port + 3, 0x03);   /* 8 bits, no parity, one stop bit */
    outb(port + 2, 0xC7);   /* Enable FIFO, clear, 14-byte threshold */
    outb(port + 4, 0x0B);   /* IRQs enabled, RTS/DSR set */

    /* Test serial chip (loopback mode) */
    outb(port + 4, 0x1E);   /* Set in loopback mode */
    outb(port + 0, 0xAE);   /* Send test byte */

    if (inb(port + 0) != 0xAE) {
        /* Serial port not functioning - continue anyway */
        return;
    }

    /* Set normal operation mode */
    outb(port + 4, 0x0F);
}

static int serial_transmit_empty(u16 port) {
    return inb(port + 5) & 0x20;
}

void serial_putchar(u16 port, char c) {
    while (!serial_transmit_empty(port));
    outb(port, c);
}

void serial_puts(u16 port, const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar(port, '\r');
        }
        serial_putchar(port, *str++);
    }
}

void serial_put_hex(u16 port, u64 value) {
    serial_puts(port, "0x");
    char buf[17];
    ultoa(value, buf, 16);
    serial_puts(port, buf);
}

int serial_received(u16 port) {
    return inb(port + 5) & 1;
}

char serial_getchar(u16 port) {
    while (!serial_received(port));
    return (char)inb(port);
}