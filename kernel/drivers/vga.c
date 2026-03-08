/*=============================================================================
 * VGA Text Mode Driver implementation
 *=============================================================================*/

#include "vga.h"
#include "../utils/string.h"
#include "../utils/stdlib.h"  /* Added for ultoa */

static volatile u16 *vga_buffer = (volatile u16 *)VGA_MEMORY;
static int  vga_row = 0;
static int  vga_col = 0;
static u8   vga_attrib = 0;

static inline u16 vga_entry(char c, u8 color) {
    return (u16)c | ((u16)color << 8);
}

static inline u8 vga_color(vga_color_t fg, vga_color_t bg) {
    return (u8)fg | ((u8)bg << 4);
}

static void update_cursor(void) {
    u16 pos = (u16)(vga_row * VGA_WIDTH + vga_col);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_attrib = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();

    /* Enable cursor (scanline 14-15) */
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void vga_clear(void) {
    u16 blank = vga_entry(' ', vga_attrib);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    vga_row = 0;
    vga_col = 0;
    update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_attrib = vga_color(fg, bg);
}

void vga_scroll(void) {
    if (vga_row < VGA_HEIGHT) return;

    /* Move all rows up by one */
    memmove((void *)vga_buffer,
            (void *)(vga_buffer + VGA_WIDTH),
            VGA_WIDTH * (VGA_HEIGHT - 1) * 2);

    /* Clear last row */
    u16 blank = vga_entry(' ', vga_attrib);
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = blank;
    }

    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    switch (c) {
        case '\n':
            vga_col = 0;
            vga_row++;
            break;
        case '\r':
            vga_col = 0;
            break;
        case '\t':
            vga_col = (vga_col + 8) & ~7;
            if (vga_col >= VGA_WIDTH) {
                vga_col = 0;
                vga_row++;
            }
            break;
        case '\b':
            if (vga_col > 0) {
                vga_col--;
                vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_attrib);
            }
            break;
        default:
            if (c >= ' ') {
                vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_attrib);
                vga_col++;
                if (vga_col >= VGA_WIDTH) {
                    vga_col = 0;
                    vga_row++;
                }
            }
            break;
    }

    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }
    update_cursor();
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_put_hex(u64 value) {
    vga_puts("0x");
    char buf[17];
    ultoa(value, buf, 16);
    /* Pad to ensure consistent width */
    int len = 0;
    char *p = buf;
    while (*p) { len++; p++; }
    for (int i = 0; i < 16 - len; i++) vga_putchar('0');
    vga_puts(buf);
}

void vga_put_dec(u64 value) {
    char buf[21];
    ultoa(value, buf, 10);
    vga_puts(buf);
}

void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH) vga_col = x;
    if (y >= 0 && y < VGA_HEIGHT) vga_row = y;
    update_cursor();
}

void vga_put_at(int x, int y, char c, u8 color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
    }
}

int vga_get_row(void) { return vga_row; }
int vga_get_col(void) { return vga_col; }