#include "dictionary.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/utils/stdlib.h"
#include "../../kernel/drivers/vga.h"
#include "../../kernel/drivers/serial.h"
#include "../../kernel/memory/pmm.h"
#include "../../kernel/memory/vmm.h"

/*=============================================================================
 * Dictionary State
 *=============================================================================*/
u8 *dict_base   = NULL;
u8 *dict_here   = NULL;   
u8 *dict_latest = NULL;   

/*=============================================================================
 * Initialize Dictionary Memory dynamically via PMM/VMM
 *=============================================================================*/
void dict_init(void) {
    dict_base   = (u8 *)DICT_VIRT_BASE;
    dict_here   = dict_base;
    dict_latest = NULL;

    /* 1. Ask PMM for contiguous physical memory */
    u64 phys_base = pmm_alloc_pages(DICT_SIZE / PAGE_SIZE);
    if (phys_base == 0) {
        kernel_panic("DICT: Out of physical memory!");
    }

    /* 2. Tell VMM to map the Virtual Address to our new Physical Address */
    if (vmm_map_range(DICT_VIRT_BASE, phys_base, DICT_SIZE, PTE_PRESENT | PTE_WRITABLE) != 0) {
        kernel_panic("DICT: Failed to map virtual memory!");
    }

    /* 3. FLUSH THE CPU TLB CACHE! */
    write_cr3(read_cr3());

    /* 4. Clear the dictionary memory safely */
    memset(dict_base, 0, DICT_SIZE);

    /* 5. Log the mapping for debugging (removed unused buf) */
    serial_puts(SERIAL_COM1, "DICT: Virtual ");
    serial_put_hex(SERIAL_COM1, DICT_VIRT_BASE);
    serial_puts(SERIAL_COM1, " mapped to PA ");
    serial_put_hex(SERIAL_COM1, phys_base);
    serial_puts(SERIAL_COM1, "\n");
}


/*=============================================================================
 * Align HERE to 8-byte boundary
 *=============================================================================*/
void dict_align(void) {
    u64 addr = (u64)dict_here;
    u64 aligned = (addr + 7) & ~7ULL;
    dict_here = (u8 *)aligned;
}

/*=============================================================================
 * Compile a cell (8 bytes) at HERE, advance HERE
 *=============================================================================*/
void dict_comma(cell_t value) {
    dict_align();
    *(cell_t *)dict_here = value;
    dict_here += sizeof(cell_t);
}

/*=============================================================================
 * Compile a single byte at HERE, advance HERE
 *=============================================================================*/
void dict_c_comma(u8 value) {
    *dict_here = value;
    dict_here++;
}

/*=============================================================================
 * Create a new word header
 *
 * Layout:
 *   [Link Pointer 8B][Flags+Length 1B][Name bytes...][Align][Code Field 8B]
 *   ^                                                       ^
 *   dict_latest points here                                  code field
 *
 * After dict_create(), dict_here points to the Code Field.
 * Caller must then set the code field and start compiling parameters.
 *=============================================================================*/
void dict_create(const char *name, u8 flags) {
    u8 name_len = (u8)strlen(name);
    if (name_len > 31) name_len = 31;

    /* Align before starting a new entry */
    dict_align();

    /* Remember where this entry starts */
    u8 *entry_start = dict_here;

    /* 1. Link Pointer (8 bytes) - points to previous word */
    *(u64 *)dict_here = (u64)dict_latest;
    dict_here += 8;

    /* 2. Flags + Length (1 byte) */
    *dict_here = (flags & 0xE0) | (name_len & F_LENMASK);
    dict_here++;

    /* 3. Name string (variable length) */
    memcpy(dict_here, name, name_len);
    dict_here += name_len;

    /* 4. Align to 8-byte boundary */
    dict_align();

    /* Update LATEST to point to this new entry */
    dict_latest = entry_start;

    /* dict_here now points where the Code Field should go.
     * The caller (dict_add_primitive or the : compiler) will
     * write the code field and parameter field from here. */
}

/*=============================================================================
 * Set the code field of the most recently created word
 * (Writes 8 bytes at current dict_here position)
 *=============================================================================*/
void dict_set_code(native_fn_t code) {
    *(cell_t *)dict_here = (cell_t)code;
    dict_here += sizeof(cell_t);
}

/*=============================================================================
 * Helper: Extract fields from a word entry
 *=============================================================================*/
u8 *dict_get_link(u8 *entry) {
    u64 link = *(u64 *)entry;
    return link ? (u8 *)link : NULL;
}

u8 dict_get_flags_len(u8 *entry) {
    return entry[8];
}

u8 dict_get_name_len(u8 *entry) {
    return entry[8] & F_LENMASK;
}

u8 dict_get_flags(u8 *entry) {
    return entry[8] & 0xE0;
}

char *dict_get_name(u8 *entry) {
    return (char *)&entry[9];
}

cell_t *dict_get_code_field(u8 *entry) {
    u8 name_len = dict_get_name_len(entry);
    /* Skip: 8 (link) + 1 (flags_len) + name_len + alignment */
    u8 *p = entry + 8 + 1 + name_len;
    /* Align to 8 */
    u64 addr = (u64)p;
    addr = (addr + 7) & ~7ULL;
    return (cell_t *)addr;
}

cell_t *dict_get_param_field(u8 *entry) {
    return dict_get_code_field(entry) + 1;
}

/*=============================================================================
 * Find a word by name (search from LATEST backwards)
 * Returns pointer to word header, or NULL if not found
 *=============================================================================*/
u8 *dict_find(const char *name) {
    u8 *entry = dict_latest;
    size_t search_len = strlen(name);

    while (entry) {
        u8 flags_len = dict_get_flags_len(entry);

        /* Skip hidden words */
        if (!(flags_len & F_HIDDEN)) {
            u8 entry_len = flags_len & F_LENMASK;
            char *entry_name = dict_get_name(entry);

            if (entry_len == search_len) {
                /* Case-insensitive comparison */
                int match = 1;
                for (u8 i = 0; i < entry_len; i++) {
                    char a = entry_name[i];
                    char b = name[i];
                    if (a >= 'a' && a <= 'z') a -= 32;
                    if (b >= 'a' && b <= 'z') b -= 32;
                    if (a != b) { match = 0; break; }
                }
                if (match) return entry;
            }
        }

        entry = dict_get_link(entry);
    }

    return NULL;
}

/*=============================================================================
 * Add a native (C) primitive to the dictionary
 * The code field points directly to the C function.
 *=============================================================================*/
void dict_add_primitive(const char *name, native_fn_t func, u8 flags) {
    dict_create(name, flags);
    dict_set_code(func);
}

/*=============================================================================
 * List all visible words
 *=============================================================================*/
void dict_list_words(void) {
    u8 *entry = dict_latest;
    int count = 0;

    while (entry) {
        u8 flags_len = dict_get_flags_len(entry);

        if (!(flags_len & F_HIDDEN)) {
            u8 name_len = flags_len & F_LENMASK;
            char *name = dict_get_name(entry);

            for (u8 i = 0; i < name_len; i++) {
                vga_putchar(name[i]);
            }
            vga_putchar(' ');
            count++;

            if (count % 10 == 0) vga_putchar('\n');
        }

        entry = dict_get_link(entry);
    }

    vga_putchar('\n');
    char buf[16];
    itoa(count, buf, 10);
    vga_puts(buf);
    vga_puts(" words total\n");
}