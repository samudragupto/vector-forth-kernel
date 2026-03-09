/*=============================================================================
 * PS/2 Keyboard Driver - IRQ1
 *=============================================================================*/

#include "keyboard.h"
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

/*--- US keyboard layout + Numpad ---*/
static const char scancode_to_ascii[] = {
    0, 27,                              
    '1','2','3','4','5','6','7','8','9','0',  
    '-','=', '\b',                     
    '\t',                              
    'q','w','e','r','t','y','u','i','o','p',  
    '[',']', '\n',                     
    0,                                 
    'a','s','d','f','g','h','j','k','l',      
    ';','\'', '`',                     
    0,                                 
    '\\',                             
    'z','x','c','v','b','n','m',       
    ',','.','/',                       
    0, '*',                            
    0, ' ',                            
    0,                                 
    /* 0x3B - 0x44 (F1-F10 keys) */
    0,0,0,0,0,0,0,0,0,0,               
    0, 0,                              /* 0x45 NumLock, 0x46 ScrollLock */
    '7','8','9', '-',                  /* 0x47-0x4A Numpad 7,8,9,- */
    '4','5','6', '+',                  /* 0x4B-0x4E Numpad 4,5,6,+ */
    '1','2','3',                       /* 0x4F-0x51 Numpad 1,2,3 */
    '0','.'                            /* 0x52-0x53 Numpad 0,. */
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
    /* F1-F10 */
    0,0,0,0,0,0,0,0,0,0,
    0, 0,
    '7','8','9', '-',
    '4','5','6', '+',
    '1','2','3',
    '0','.'
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
        c += 32;
    }

    if (c == 0) return;

    /* Ctrl+key combinations */
    if (ctrl_pressed) {
        if (c >= 'a' && c <= 'z') c -= 96;  
        else if (c >= 'A' && c <= 'Z') c -= 64;
    }

    /* Put in buffer - DO NOT PRINT TO SCREEN HERE */
    kbd_buffer_put(c);
}

/*--- Initialize keyboard ---*/
void keyboard_init(void) {
    /* Flush keyboard buffer */
    while (inb(KBD_STATUS_PORT) & 1) {
        inb(KBD_DATA_PORT);
    }
}