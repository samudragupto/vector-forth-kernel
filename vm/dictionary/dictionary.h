#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "../../kernel/core/kernel.h"

/*=============================================================================
 * Dictionary Memory Layout (matches architecture diagram):
 *
 * Each entry in the flat memory dictionary:
 * +0x00: Link Pointer     (8 bytes) - points to previous word
 * +0x08: Flags + Length    (1 byte)  - flags in upper bits, length in lower 5
 * +0x09: Name              (variable) - name string
 * +???:  Padding           (to 8-byte alignment)
 * +???:  Code Field        (8 bytes) - pointer to code (DOCOL, native, etc.)
 * +???:  Parameter Field   (variable) - compiled word addresses or data
 *=============================================================================*/

/* Flags stored in upper bits of the flags_len byte */
#define F_IMMEDIATE  0x80
#define F_HIDDEN     0x40
#define F_LENMASK    0x1F   /* Lower 5 bits = name length (max 31) */

/* Dictionary memory region */
#define DICT_VIRT_BASE  0xFFFF800010000000ULL
#define DICT_SIZE       (1 * 1024 * 1024)  /* 16MB */

/* A "cell" is 8 bytes (64-bit) */
typedef u64 cell_t;
typedef i64 scell_t;

/*--- Code field function type ---*/
/* Native C primitives receive no arguments; they operate on the stacks */
typedef void (*native_fn_t)(void);

/*--- Dictionary Pointer (HERE) ---*/
/* Points to the next free byte in dictionary memory */
extern u8  *dict_here;
extern u8  *dict_latest;  /* Points to the most recent word header */
extern u8  *dict_base;    /* Base of dictionary memory */

/*--- Initialize dictionary memory ---*/
void dict_init(void);

/*--- Create a new word header in dictionary ---*/
void dict_create(const char *name, u8 flags);

/*--- Compile a cell (8 bytes) into HERE and advance ---*/
void dict_comma(cell_t value);

/*--- Compile a byte into HERE and advance ---*/
void dict_c_comma(u8 value);

/*--- Align HERE to 8-byte boundary ---*/
void dict_align(void);

/*--- Set the code field of the most recent word ---*/
void dict_set_code(native_fn_t code);

/*--- Word lookup ---*/
u8  *dict_find(const char *name);

/*--- Extract fields from a word header ---*/
u8  *dict_get_link(u8 *entry);
u8   dict_get_flags_len(u8 *entry);
char *dict_get_name(u8 *entry);
u8   dict_get_name_len(u8 *entry);
u8   dict_get_flags(u8 *entry);
cell_t *dict_get_code_field(u8 *entry);
cell_t *dict_get_param_field(u8 *entry);

/*--- List all visible words ---*/
void dict_list_words(void);

/*--- Add a native (C) primitive to the dictionary ---*/
void dict_add_primitive(const char *name, native_fn_t func, u8 flags);

#endif