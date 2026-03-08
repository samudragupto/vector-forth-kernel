#include "dictionary.h"
#include "../../kernel/memory/heap.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/utils/stdlib.h"  /* <-- Added this include for itoa */
#include "../../kernel/drivers/vga.h"
#include "../../kernel/drivers/serial.h"

static dict_entry_t *latest = NULL;

void dict_init(void) {
    latest = NULL;
}

void dict_add_word(const char *name, code_ptr_t func, u8 flags) {
    dict_entry_t *w = (dict_entry_t *)kmalloc(sizeof(dict_entry_t));
    if (!w) {
        kernel_panic("Dictionary: Out of memory!");
    }

    /* Link to previous word */
    w->link = latest;

    /* Copy name (case-preserved) */
    size_t len = strlen(name);
    if (len >= MAX_WORD_NAME) len = MAX_WORD_NAME - 1;
    strncpy(w->name, name, len);
    w->name[len] = '\0';

    w->length = (u8)len;
    w->flags  = flags;
    w->code   = func;

    latest = w;

    serial_puts(SERIAL_COM1, "DICT: Added '");
    serial_puts(SERIAL_COM1, name);
    serial_puts(SERIAL_COM1, "'\n");
}

dict_entry_t *dict_find(const char *name) {
    dict_entry_t *w = latest;
    while (w) {
        /* Skip hidden words */
        if (!(w->flags & FLAG_HIDDEN)) {
            if (stricmp(name, w->name) == 0) {
                return w;
            }
        }
        w = w->link;
    }
    return NULL;
}

dict_entry_t *dict_latest(void) {
    return latest;
}

void dict_list_words(void) {
    dict_entry_t *w = latest;
    int count = 0;

    while (w) {
        if (!(w->flags & FLAG_HIDDEN)) {
            vga_puts(w->name);
            vga_putchar(' ');
            count++;

            /* Line wrap every 10 words */
            if (count % 10 == 0) {
                vga_putchar('\n');
            }
        }
        w = w->link;
    }

    vga_putchar('\n');
    
    char buf[16];
    itoa(count, buf, 10);
    vga_puts(buf);
    vga_puts(" words total\n");
}