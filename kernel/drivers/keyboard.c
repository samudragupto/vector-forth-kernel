/*=============================================================================
 * PS/2 Keyboard Driver - IRQ1
 *=============================================================================*/

#include "../core/kernel.h"
#include "vga.h"
#include "serial.h"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

/*--- Keyboard buffer ---*/
#define KBD_BUFFER_SIZE 256
static volatile char    kbd_buffer[KBD_BUFFER_SIZE];
static volatile u32     kbd_head = 0;
static volatile u32     kbd_tail = 0;

/*--- Modifier state ---*/
static volatile u8 shift_pressed = 0;
static volatile u8 ctrl_pressed  = 0;
static volatile u8 alt_pressed   = 0;
static volatile u8 caps_lock     = 0;

/*--- US keyboard layout (scancode set 1) ---*/
static const char scancode_to_ascii[] = {
    0, 0,                              /* 0x00, 0x01 (ESC) */
    '1','2','3','4','5','6','7','8','9','0',  /* 0x02-0x0B */
    '-','=', '\b',                     /* 0x0C-0x0E */
    '\t',                              /* 0x0F */
    'q','w','e','r','t','y','u','i','o','p',  /* 0x10-0x19 */
    '[',']', '\n',                     /* 0x1A-0x1C */
    0,                                 /* 0x1D (Left Ctrl) */
    'a','s','d','f','g','h','j','k','l',      /* 0x1E-0x26 */
    ';','\'', '`',                     /* 0x27-0x29 */
    0,                                 /* 0x2A (Left Shift) */
    '\\',                             /* 0x2B */
    'z','x','c','v','b','n','m',       /* 0x2C-0x32 */
    ',','.','/',                       /* 0x33-0x35 */
    0, '*',                            /* 0x36 (Right Shift), 0x37 */
    0, ' ',                            /* 0x38 (Left Alt), 0x39 (Space) */
    0,                                 /* 0x3A (Caps Lock) */
};

static const char scancode_to_ascii_shift[] = {
    0, 0,
    '!','@','#','$','%','^','&','*','(',')',
    '_','+', '\b',
    '\t',
    'Q','W','E','R','T','Y','U','I','O','P',
    '{','}', '\n',
    0,
    'A','S','D','F','G','H','J','K','L',
    ':','"', '~',
    0,
    '|',
    'Z','X','C','V','B','N','M',
    '<','>','?',
    0, '*',
    0, ' ',
    0,
};

#define SCANCODE_TABLE_SIZE (sizeof(scancode_to_ascii) / sizeof(scancode_to_ascii[0]))

/*--- Buffer operations ---*/
static void kbd_buffer_put(char c) {
    u32 next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

/*--- Public: read character from buffer (blocking) ---*/
char keyboard_getchar(void) {
    while (kbd_head == kbd_tail) {
        hlt();
    }
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

/*--- Public: check if character available ---*/
int keyboard_available(void) {
    return kbd_head != kbd_tail;
}

/*--- IRQ1 handler (called from irq.c) ---*/
void keyboard_handler_c(void) {
    u8 scancode = inb(KBD_DATA_PORT);

    /* Key release (high bit set) */
    if (scancode & 0x80) {
        u8 released = scancode & 0x7F;
        switch (released) {
            case 0x2A: case 0x36: shift_pressed = 0; break;
            case 0x1D: ctrl_pressed = 0; break;
            case 0x38: alt_pressed = 0; break;
        }
        return;
    }

    /* Key press */
    switch (scancode) {
        case 0x2A: case 0x36: shift_pressed = 1; return;
        case 0x1D: ctrl_pressed = 1; return;
        case 0x38: alt_pressed = 1; return;
        case 0x3A: caps_lock = !caps_lock; return;
    }

    if (scancode >= SCANCODE_TABLE_SIZE) return;

    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    /* Apply caps lock to alpha characters */
    if (caps_lock && c >= 'a' && c <= 'z') {
        c -= 32;
    } else if (caps_lock && c >= 'A' && c <= 'Z' && !shift_pressed) {
        /* caps lock with shift = lowercase */
    }

    if (c == 0) return;

    /* Ctrl+key combinations */
    if (ctrl_pressed) {
        if (c >= 'a' && c <= 'z') c -= 96;  /* Ctrl+A = 1, etc. */
        else if (c >= 'A' && c <= 'Z') c -= 64;
    }

    /* Put in buffer */
    kbd_buffer_put(c);

    /* Echo to screen */
    vga_putchar(c);

    /* Debug: echo to serial */
    serial_putchar(SERIAL_COM1, c);
}

/*--- Initialize keyboard ---*/
void keyboard_init(void) {
    /* Flush keyboard buffer */
    while (inb(KBD_STATUS_PORT) & 1) {
        inb(KBD_DATA_PORT);
    }
}