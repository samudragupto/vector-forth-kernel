#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "../../kernel/core/kernel.h"

#define MAX_WORD_NAME 32

typedef void (*code_ptr_t)(void);

/*--- Dictionary Entry (Word Header) ---*/
typedef struct dict_entry {
    struct dict_entry *link;        /* Previous word in linked list */
    u8                 flags;       /* IMMEDIATE, HIDDEN, etc. */
    u8                 length;      /* Name length */
    char               name[MAX_WORD_NAME]; /* Name string */
    code_ptr_t         code;        /* Function pointer to execute */
} dict_entry_t;

/*--- Word Flags ---*/
#define FLAG_IMMEDIATE  0x80
#define FLAG_HIDDEN     0x40
#define FLAG_COMPILE    0x20

/*--- Functions ---*/
void          dict_init(void);
void          dict_add_word(const char *name, code_ptr_t func, u8 flags);
dict_entry_t *dict_find(const char *name);
dict_entry_t *dict_latest(void);
void          dict_list_words(void);

#endif