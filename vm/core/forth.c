#include "forth.h"
#include "../stack/stack.h"
#include "../dictionary/dictionary.h"
#include "../../kernel/drivers/vga.h"
#include "../../kernel/drivers/serial.h"
#include "../../kernel/drivers/keyboard.h"
#include "../../kernel/utils/stdlib.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/core/kernel.h"
#include "../../kernel/memory/heap.h" /* Added for heap stats */
#include "../../kernel/memory/pmm.h"  /* Added for PMM stats */

/*=============================================================================
 * Interpreter State
 *=============================================================================*/
static int forth_state = STATE_INTERPRET;
static int forth_base  = 10;

int  forth_get_state(void) { return forth_state; }
void forth_set_state(int s) { forth_state = s; }
int  forth_get_base(void)  { return forth_base; }
void forth_set_base(int b) { forth_base = b; }

/*=============================================================================
 * Input Buffer
 *=============================================================================*/
#define INPUT_BUF_SIZE 256
static char input_buffer[INPUT_BUF_SIZE];
static int  input_pos = 0;
/* Removed unused input_len variable */

/* The current line being parsed (set by forth_eval) */
static const char *parse_ptr = NULL;

/* Parse the next whitespace-delimited token from the input */
static int parse_word(char *out, int max_len) {
    /* Skip whitespace */
    while (*parse_ptr && isspace(*parse_ptr)) parse_ptr++;
    if (!*parse_ptr) return 0;

    int len = 0;
    while (*parse_ptr && !isspace(*parse_ptr) && len < max_len - 1) {
        out[len++] = *parse_ptr++;
    }
    out[len] = '\0';
    return len;
}

/* Parse up to a specific delimiter (for ." and ( ) */
static int parse_until(char delim, char *out, int max_len) {
    /* Skip one leading space if present */
    if (*parse_ptr == ' ') parse_ptr++;
    
    int len = 0;
    while (*parse_ptr && *parse_ptr != delim && len < max_len - 1) {
        out[len++] = *parse_ptr++;
    }
    out[len] = '\0';
    
    /* Skip the delimiter */
    if (*parse_ptr == delim) parse_ptr++;
    return len;
}

/*=============================================================================
 * Number Parsing
 *=============================================================================*/
static int try_parse_number(const char *word, i64 *out) {
    char *end;
    int base = forth_base;

    /* Handle hex prefix */
    if (word[0] == '0' && (word[1] == 'x' || word[1] == 'X')) {
        base = 16;
        word += 2;
    }
    /* Handle '$' hex prefix (Forth convention) */
    else if (word[0] == '$') {
        base = 16;
        word++;
    }
    /* Handle '#' decimal prefix */
    else if (word[0] == '#') {
        base = 10;
        word++;
    }
    /* Handle '%' binary prefix */
    else if (word[0] == '%') {
        base = 2;
        word++;
    }

    long val = strtol(word, &end, base);
    if (*end == '\0' && end != word) {
        *out = (i64)val;
        return 1;
    }
    return 0;
}

/*=============================================================================
 * STACK MANIPULATION PRIMITIVES
 *=============================================================================*/
static void w_dup(void)  { ds_push(ds_peek()); }
static void w_drop(void) { ds_pop(); }

static void w_swap(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(b);
    ds_push(a);
}

static void w_over(void) {
    ds_push(ds_pick(1));
}

static void w_rot(void) {
    i64 c = ds_pop();
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(b);
    ds_push(c);
    ds_push(a);
}

static void w_nip(void) {
    i64 top = ds_pop();
    ds_pop();
    ds_push(top);
}

static void w_tuck(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(b);
    ds_push(a);
    ds_push(b);
}

static void w_2dup(void) {
    i64 b = ds_peek();
    i64 a = ds_pick(1);
    ds_push(a);
    ds_push(b);
}

static void w_2drop(void) {
    ds_pop();
    ds_pop();
}

static void w_2swap(void) {
    i64 d = ds_pop();
    i64 c = ds_pop();
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(c);
    ds_push(d);
    ds_push(a);
    ds_push(b);
}

static void w_qdup(void) {
    i64 top = ds_peek();
    if (top != 0) ds_push(top);
}

static void w_depth(void) {
    ds_push(ds_depth());
}

/*=============================================================================
 * ARITHMETIC PRIMITIVES
 *=============================================================================*/
static void w_plus(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a + b);
}

static void w_minus(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a - b);
}

static void w_mul(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a * b);
}

static void w_div(void) {
    i64 b = ds_pop();
    if (b == 0) {
        vga_puts(" Division by zero!\n");
        ds_push(0);
        return;
    }
    i64 a = ds_pop();
    ds_push(a / b);
}

static void w_mod(void) {
    i64 b = ds_pop();
    if (b == 0) {
        vga_puts(" Division by zero!\n");
        ds_push(0);
        return;
    }
    i64 a = ds_pop();
    ds_push(a % b);
}

static void w_divmod(void) {
    i64 b = ds_pop();
    if (b == 0) {
        vga_puts(" Division by zero!\n");
        ds_push(0);
        ds_push(0);
        return;
    }
    i64 a = ds_pop();
    ds_push(a % b);
    ds_push(a / b);
}

static void w_negate(void) {
    ds_push(-ds_pop());
}

static void w_abs(void) {
    i64 val = ds_pop();
    ds_push(val < 0 ? -val : val);
}

static void w_min(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a < b ? a : b);
}

static void w_max(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a > b ? a : b);
}

static void w_1plus(void)  { ds_push(ds_pop() + 1); }
static void w_1minus(void) { ds_push(ds_pop() - 1); }
static void w_2star(void)  { ds_push(ds_pop() << 1); }
static void w_2slash(void) { ds_push(ds_pop() >> 1); }

/*=============================================================================
 * LOGIC / COMPARISON PRIMITIVES
 *=============================================================================*/
static void w_and(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a & b);
}

static void w_or(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a | b);
}

static void w_xor(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a ^ b);
}

static void w_invert(void) {
    ds_push(~ds_pop());
}

static void w_lshift(void) {
    i64 n = ds_pop();
    i64 val = ds_pop();
    ds_push(val << n);
}

static void w_rshift(void) {
    i64 n = ds_pop();
    i64 val = ds_pop();
    ds_push((u64)val >> n);
}

/* Forth truth: 0 = false, -1 (all bits set) = true */
static void w_eq(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a == b ? -1 : 0);
}

static void w_neq(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a != b ? -1 : 0);
}

static void w_lt(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a < b ? -1 : 0);
}

static void w_gt(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a > b ? -1 : 0);
}

static void w_le(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a <= b ? -1 : 0);
}

static void w_ge(void) {
    i64 b = ds_pop();
    i64 a = ds_pop();
    ds_push(a >= b ? -1 : 0);
}

static void w_0eq(void) {
    ds_push(ds_pop() == 0 ? -1 : 0);
}

static void w_0lt(void) {
    ds_push(ds_pop() < 0 ? -1 : 0);
}

static void w_0gt(void) {
    ds_push(ds_pop() > 0 ? -1 : 0);
}

/*=============================================================================
 * RETURN STACK PRIMITIVES
 *=============================================================================*/
static void w_to_r(void) {
    rs_push((u64)ds_pop());
}

static void w_from_r(void) {
    ds_push((i64)rs_pop());
}

static void w_r_fetch(void) {
    ds_push((i64)rs_peek());
}

/*=============================================================================
 * MEMORY PRIMITIVES
 *=============================================================================*/
static void w_fetch(void) {
    u64 *addr = (u64 *)ds_pop();
    ds_push((i64)*addr);
}

static void w_store(void) {
    u64 *addr = (u64 *)ds_pop();
    i64 val = ds_pop();
    *addr = (u64)val;
}

static void w_cfetch(void) {
    u8 *addr = (u8 *)ds_pop();
    ds_push((i64)*addr);
}

static void w_cstore(void) {
    u8 *addr = (u8 *)ds_pop();
    i64 val = ds_pop();
    *addr = (u8)val;
}

static void w_plus_store(void) {
    u64 *addr = (u64 *)ds_pop();
    i64 val = ds_pop();
    *addr += val;
}

/*=============================================================================
 * OUTPUT PRIMITIVES
 *=============================================================================*/
static void w_dot(void) {
    i64 val = ds_pop();
    char buf[24];
    ltoa((long)val, buf, forth_base);
    vga_puts(buf);
    vga_putchar(' ');
}

static void w_u_dot(void) {
    u64 val = (u64)ds_pop();
    char buf[24];
    ultoa(val, buf, forth_base);
    vga_puts(buf);
    vga_putchar(' ');
}

static void w_dot_s(void) {
    ds_print();
}

static void w_emit(void) {
    char c = (char)ds_pop();
    vga_putchar(c);
}

static void w_cr(void) {
    vga_putchar('\n');
}

static void w_space(void) {
    vga_putchar(' ');
}

static void w_spaces(void) {
    i64 n = ds_pop();
    for (i64 i = 0; i < n; i++) {
        vga_putchar(' ');
    }
}

static void w_type(void) {
    i64 len = ds_pop();
    char *addr = (char *)ds_pop();
    for (i64 i = 0; i < len; i++) {
        vga_putchar(addr[i]);
    }
}

static void w_dot_quote(void) {
    char buf[128];
    parse_until('"', buf, sizeof(buf));
    vga_puts(buf);
}

static void w_paren(void) {
    char buf[256];
    parse_until(')', buf, sizeof(buf));
}

static void w_dot_hex(void) {
    i64 val = ds_pop();
    vga_put_hex((u64)val);
    vga_putchar(' ');
}

/*=============================================================================
 * SYSTEM / UTILITY PRIMITIVES
 *=============================================================================*/
static void w_words(void) {
    dict_list_words();
}

static void w_bye(void) {
    vga_puts("\nSystem halted.\n");
    cli();
    while (1) hlt();
}

static void w_clear(void) {
    vga_clear();
}

static void w_base(void) {
    ds_push((i64)forth_base);
}

static void w_decimal(void) {
    forth_base = 10;
}

static void w_hex(void) {
    forth_base = 16;
}

static void w_ticks(void) {
    extern volatile u64 kernel_ticks;
    ds_push((i64)kernel_ticks);
}

static void w_free(void) {
    ds_push((i64)heap_get_free());
}

static void w_status(void) {
    char buf[24];
    extern volatile u64 kernel_ticks;

    vga_puts("=== System Status ===\n");

    vga_puts("kernel_main: ");
    extern void kernel_main(u64, e820_entry_t *);
    vga_put_hex((u64)&kernel_main);
    vga_putchar('\n');

    vga_puts("Ticks:       ");
    ultoa(kernel_ticks, buf, 10);
    vga_puts(buf);
    vga_putchar('\n');

    vga_puts("Heap total:  ");
    ultoa(heap_get_total(), buf, 10);
    vga_puts(buf);
    vga_puts(" bytes\n");

    vga_puts("Heap used:   ");
    ultoa(heap_get_used(), buf, 10);
    vga_puts(buf);
    vga_puts(" bytes\n");

    vga_puts("Heap free:   ");
    ultoa(heap_get_free(), buf, 10);
    vga_puts(buf);
    vga_puts(" bytes\n");

    vga_puts("PMM free:    ");
    ultoa(pmm_get_free_memory() / 1024, buf, 10);
    vga_puts(buf);
    vga_puts(" KB\n");

    vga_puts("Stack depth: ");
    itoa((int)ds_depth(), buf, 10);
    vga_puts(buf);
    vga_putchar('\n');

    vga_puts("Base:        ");
    itoa(forth_base, buf, 10);
    vga_puts(buf);
    vga_putchar('\n');
}

static void w_dump(void) {
    i64 count = ds_pop();
    u8 *addr = (u8 *)ds_pop();
    char buf[8];

    for (i64 i = 0; i < count; i++) {
        if (i % 16 == 0) {
            vga_put_hex((u64)(addr + i));
            vga_puts(": ");
        }

        utoa(addr[i], buf, 16);
        if (addr[i] < 16) vga_putchar('0');
        vga_puts(buf);
        vga_putchar(' ');

        if (i % 16 == 15) {
            vga_puts(" | ");
            for (int j = 15; j >= 0; j--) {
                char c = addr[i - j];
                vga_putchar((c >= 32 && c < 127) ? c : '.');
            }
            vga_putchar('\n');
        }
    }
    if (count % 16 != 0) vga_putchar('\n');
}

static void w_help(void) {
    vga_puts("=== Vector Forth Help ===\n");
    vga_puts("Stack:   DUP DROP SWAP OVER ROT NIP TUCK .S DEPTH\n");
    vga_puts("Math:    + - * / MOD NEGATE ABS MIN MAX 1+ 1-\n");
    vga_puts("Logic:   AND OR XOR INVERT LSHIFT RSHIFT\n");
    vga_puts("Compare: = <> < > <= >= 0= 0< 0>\n");
    vga_puts("Memory:  @ ! C@ C! +!\n");
    vga_puts("Output:  . U. .S .HEX EMIT CR SPACE SPACES .\" \n");
    vga_puts("Return:  >R R> R@\n");
    vga_puts("System:  WORDS HELP STATUS FREE TICKS DUMP\n");
    vga_puts("Base:    DECIMAL HEX BASE\n");
    vga_puts("Other:   CLEAR BYE\n");
    vga_puts("\nExamples:\n");
    vga_puts("  10 20 + .          ( prints 30 )\n");
    vga_puts("  1 2 3 .S           ( shows stack )\n");
    vga_puts("  65 EMIT            ( prints 'A' )\n");
    vga_puts("  .\" Hello\"          ( prints text )\n");
    vga_puts("  HEX FF .           ( prints 255 in hex )\n");
    vga_puts("  STATUS             ( system info )\n");
}

/*=============================================================================
 * Register All Primitives
 *=============================================================================*/
static void register_primitives(void) {
    dict_add_word("DUP",    w_dup,    0);
    dict_add_word("DROP",   w_drop,   0);
    dict_add_word("SWAP",   w_swap,   0);
    dict_add_word("OVER",   w_over,   0);
    dict_add_word("ROT",    w_rot,    0);
    dict_add_word("NIP",    w_nip,    0);
    dict_add_word("TUCK",   w_tuck,   0);
    dict_add_word("2DUP",   w_2dup,   0);
    dict_add_word("2DROP",  w_2drop,  0);
    dict_add_word("2SWAP",  w_2swap,  0);
    dict_add_word("?DUP",   w_qdup,   0);
    dict_add_word("DEPTH",  w_depth,  0);

    dict_add_word("+",      w_plus,   0);
    dict_add_word("-",      w_minus,  0);
    dict_add_word("*",      w_mul,    0);
    dict_add_word("/",      w_div,    0);
    dict_add_word("MOD",    w_mod,    0);
    dict_add_word("/MOD",   w_divmod, 0);
    dict_add_word("NEGATE", w_negate, 0);
    dict_add_word("ABS",    w_abs,    0);
    dict_add_word("MIN",    w_min,    0);
    dict_add_word("MAX",    w_max,    0);
    dict_add_word("1+",     w_1plus,  0);
    dict_add_word("1-",     w_1minus, 0);
    dict_add_word("2*",     w_2star,  0);
    dict_add_word("2/",     w_2slash, 0);

    dict_add_word("AND",    w_and,    0);
    dict_add_word("OR",     w_or,     0);
    dict_add_word("XOR",    w_xor,    0);
    dict_add_word("INVERT", w_invert, 0);
    dict_add_word("LSHIFT", w_lshift, 0);
    dict_add_word("RSHIFT", w_rshift, 0);
    dict_add_word("=",      w_eq,     0);
    dict_add_word("<>",     w_neq,    0);
    dict_add_word("<",      w_lt,     0);
    dict_add_word(">",      w_gt,     0);
    dict_add_word("<=",     w_le,     0);
    dict_add_word(">=",     w_ge,     0);
    dict_add_word("0=",     w_0eq,    0);
    dict_add_word("0<",     w_0lt,    0);
    dict_add_word("0>",     w_0gt,    0);

    dict_add_word(">R",     w_to_r,   0);
    dict_add_word("R>",     w_from_r, 0);
    dict_add_word("R@",     w_r_fetch,0);

    dict_add_word("@",      w_fetch,  0);
    dict_add_word("!",      w_store,  0);
    dict_add_word("C@",     w_cfetch, 0);
    dict_add_word("C!",     w_cstore, 0);
    dict_add_word("+!",     w_plus_store, 0);

    dict_add_word(".",      w_dot,    0);
    dict_add_word("U.",     w_u_dot,  0);
    dict_add_word(".S",     w_dot_s,  0);
    dict_add_word(".HEX",   w_dot_hex,0);
    dict_add_word("EMIT",   w_emit,   0);
    dict_add_word("CR",     w_cr,     0);
    dict_add_word("SPACE",  w_space,  0);
    dict_add_word("SPACES", w_spaces, 0);
    dict_add_word("TYPE",   w_type,   0);
    dict_add_word(".\"",    w_dot_quote, FLAG_IMMEDIATE);
    dict_add_word("(",      w_paren,  FLAG_IMMEDIATE);

    dict_add_word("WORDS",  w_words,  0);
    dict_add_word("HELP",   w_help,   0);
    dict_add_word("STATUS", w_status, 0);
    dict_add_word("FREE",   w_free,   0);
    dict_add_word("TICKS",  w_ticks,  0);
    dict_add_word("DUMP",   w_dump,   0);
    dict_add_word("CLEAR",  w_clear,  0);
    dict_add_word("BYE",    w_bye,    0);
    dict_add_word("BASE",   w_base,   0); /* Fixed: w_base registered here */
    dict_add_word("DECIMAL",w_decimal,0);
    dict_add_word("HEX",    w_hex,    0);
}

/*=============================================================================
 * Outer Interpreter (Evaluate a line of input)
 *=============================================================================*/
void forth_eval(const char *line) {
    char token[64];
    parse_ptr = line;

    while (parse_word(token, sizeof(token))) {
        dict_entry_t *word = dict_find(token);
        if (word) {
            word->code();
            continue;
        }

        i64 num;
        if (try_parse_number(token, &num)) {
            ds_push(num);
            continue;
        }

        vga_puts(token);
        vga_puts(" ? Unknown word\n");
        return;
    }

    vga_puts(" ok\n");
}

/*=============================================================================
 * Initialize Forth VM
 *=============================================================================*/
void forth_init(void) {
    ds_clear();
    rs_clear();
    dict_init();
    register_primitives();

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("\n  Vector Forth OS v0.1\n");
    vga_puts("  Type HELP for commands\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    serial_puts(SERIAL_COM1, "Forth VM initialized\n");
}

/*=============================================================================
 * REPL (Read-Eval-Print Loop)
 *=============================================================================*/
void forth_run(void) {
    input_pos = 0;

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    while (1) {
        if (keyboard_available()) {
            char c = keyboard_getchar();

            if (c == '\n') {
                input_buffer[input_pos] = '\0';
                vga_putchar('\n');

                if (input_pos > 0) {
                    serial_puts(SERIAL_COM1, ">> ");
                    serial_puts(SERIAL_COM1, input_buffer);
                    serial_puts(SERIAL_COM1, "\n");
                    forth_eval(input_buffer);
                }

                input_pos = 0;
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts("> ");
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
            else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                }
            }
            else if (c >= 32 && input_pos < INPUT_BUF_SIZE - 1) {
                input_buffer[input_pos++] = c;
            }
        }
        hlt();
    }
}