#include "forth.h"
#include "../stack/stack.h"
#include "../dictionary/dictionary.h"
#include "../../kernel/drivers/vga.h"
#include "../../kernel/drivers/serial.h"
#include "../../kernel/drivers/keyboard.h"
#include "../../kernel/utils/stdlib.h"
#include "../../kernel/utils/string.h"
#include "../../kernel/core/kernel.h"
#include "../../kernel/memory/heap.h"
#include "../../kernel/memory/pmm.h"

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
 * Input Buffer & Parser
 *=============================================================================*/
#define INPUT_BUF_SIZE 256
static char input_buffer[INPUT_BUF_SIZE];
static int  input_pos = 0;

static const char *parse_ptr = NULL;

static int parse_word(char *out, int max_len) {
    while (*parse_ptr && isspace(*parse_ptr)) parse_ptr++;
    if (!*parse_ptr) return 0;

    int len = 0;
    while (*parse_ptr && !isspace(*parse_ptr) && len < max_len - 1) {
        out[len++] = *parse_ptr++;
    }
    out[len] = '\0';
    return len;
}

static int parse_until(char delim, char *out, int max_len) {
    if (*parse_ptr == ' ') parse_ptr++;

    int len = 0;
    while (*parse_ptr && *parse_ptr != delim && len < max_len - 1) {
        out[len++] = *parse_ptr++;
    }
    out[len] = '\0';
    if (*parse_ptr == delim) parse_ptr++;
    return len;
}

static int try_parse_number(const char *word, i64 *out) {
    char *end;
    int base = forth_base;

    if (word[0] == '0' && (word[1] == 'x' || word[1] == 'X')) {
        base = 16; word += 2;
    } else if (word[0] == '$') {
        base = 16; word++;
    } else if (word[0] == '#') {
        base = 10; word++;
    } else if (word[0] == '%') {
        base = 2; word++;
    }

    long val = strtol(word, &end, base);
    if (*end == '\0' && end != word) {
        *out = (i64)val;
        return 1;
    }
    return 0;
}

/*=============================================================================
 * Inner Interpreter: DOCOL, EXIT, NEXT, LITERAL
 *
 * DOCOL (Do Colon):
 *   Called when a colon-defined word is executed.
 *   Pushes the current IP onto the Return Stack, sets IP to the
 *   Parameter Field of the word, then runs NEXT.
 *
 * EXIT:
 *   Pops the Return Stack into IP, resumes the caller.
 *
 * NEXT:
 *   Fetches the cell at *IP, advances IP, and executes it.
 *   If the cell is a dictionary entry address, it looks up the code field
 *   and calls it. This is the heartbeat of the Forth VM.
 *
 * LITERAL:
 *   The next cell in the parameter field is a number.
 *   Push it onto the data stack and advance IP.
 *=============================================================================*/

/* Sentinel value to stop the inner interpreter */
#define SENTINEL_EXIT 0xDEADC0DE00000000ULL

/* Forward declaration */
static void forth_inner(void);

/* DOCOL: Enter a colon definition */
void do_colon(void) {
    /* This function is called via forth_execute().
     * At this point, we know the word's parameter field.
     * The caller (forth_execute) has already set up what we need.
     * We implement this inline in forth_execute. */
}

/* Do LITERAL: Push the next cell as a number */
void do_literal(void) {
    u64 val = ip_fetch_advance();
    ds_push((i64)val);
}

/* Execute a word given its entry pointer */
void forth_execute(u8 *entry) {
    cell_t *code_field = dict_get_code_field(entry);
    cell_t code = *code_field;

    if (code == (cell_t)do_colon) {
        /* It's a colon-defined word. Enter it. */
        /* Push current IP to return stack */
        rs_push((u64)ip_get());

        /* Set IP to parameter field */
        cell_t *pfa = dict_get_param_field(entry);
        ip_set((u64 *)pfa);

        /* Run the inner interpreter */
        forth_inner();
    } else {
        /* It's a native C primitive. Just call it. */
        native_fn_t fn = (native_fn_t)code;
        fn();
    }
}

/* The Inner Interpreter Loop (NEXT) */
static void forth_inner(void) {
    while (1) {
        u64 cell = ip_fetch_advance();

        /* EXIT sentinel */
        if (cell == SENTINEL_EXIT) {
            /* Pop return stack to restore caller's IP */
            u64 saved_ip = rs_pop();
            ip_set((u64 *)saved_ip);

            /* If saved_ip is NULL, we've returned from the top-level */
            if (saved_ip == 0) return;
            continue;
        }

        /* LITERAL marker */
        if (cell == (u64)do_literal) {
            u64 val = ip_fetch_advance();
            ds_push((i64)val);
            continue;
        }

        /* Otherwise, cell is a pointer to a dictionary entry */
        u8 *entry = (u8 *)cell;

        /* Safety check: is this a valid dictionary address? */
        if ((u64)entry < DICT_VIRT_BASE ||
            (u64)entry >= DICT_VIRT_BASE + DICT_SIZE) {
            vga_puts(" Invalid word address in compiled code!\n");
            return;
        }

        cell_t *code_field = dict_get_code_field(entry);
        cell_t code = *code_field;

        if (code == (cell_t)do_colon) {
            /* Nested colon word: push IP, enter it */
            rs_push((u64)ip_get());
            cell_t *pfa = dict_get_param_field(entry);
            ip_set((u64 *)pfa);
            /* Continue the loop (this IS next) */
        } else {
            /* Native primitive */
            native_fn_t fn = (native_fn_t)code;
            fn();
        }
    }
}

/*=============================================================================
 * STACK MANIPULATION PRIMITIVES
 *=============================================================================*/
static void w_dup(void)  { ds_push(ds_peek()); }
static void w_drop(void) { ds_pop(); }
static void w_swap(void) { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(b); ds_push(a); }
static void w_over(void) { ds_push(ds_pick(1)); }
static void w_rot(void)  { i64 c=ds_pop(); i64 b=ds_pop(); i64 a=ds_pop();
                            ds_push(b); ds_push(c); ds_push(a); }
static void w_nip(void)  { i64 t=ds_pop(); ds_pop(); ds_push(t); }
static void w_tuck(void) { i64 b=ds_pop(); i64 a=ds_pop();
                            ds_push(b); ds_push(a); ds_push(b); }
static void w_2dup(void) { i64 b=ds_peek(); i64 a=ds_pick(1);
                            ds_push(a); ds_push(b); }
static void w_2drop(void){ ds_pop(); ds_pop(); }
static void w_2swap(void){ i64 d=ds_pop(); i64 c=ds_pop();
                            i64 b=ds_pop(); i64 a=ds_pop();
                            ds_push(c); ds_push(d); ds_push(a); ds_push(b); }
static void w_qdup(void) { i64 t=ds_peek(); if(t) ds_push(t); }
static void w_depth(void){ ds_push(ds_depth()); }

/*=============================================================================
 * ARITHMETIC PRIMITIVES
 *=============================================================================*/
static void w_plus(void) { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a+b); }
static void w_minus(void){ i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a-b); }
static void w_mul(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a*b); }
static void w_div(void)  { i64 b=ds_pop(); if(!b){vga_puts(" Div/0!\n");ds_push(0);return;}
                            i64 a=ds_pop(); ds_push(a/b); }
static void w_mod(void)  { i64 b=ds_pop(); if(!b){vga_puts(" Div/0!\n");ds_push(0);return;}
                            i64 a=ds_pop(); ds_push(a%b); }
static void w_divmod(void){ i64 b=ds_pop();
                            if(!b){vga_puts(" Div/0!\n");ds_push(0);ds_push(0);return;}
                            i64 a=ds_pop(); ds_push(a%b); ds_push(a/b); }
static void w_negate(void){ ds_push(-ds_pop()); }
static void w_abs(void)  { i64 v=ds_pop(); ds_push(v<0?-v:v); }
static void w_min(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a<b?a:b); }
static void w_max(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a>b?a:b); }
static void w_1plus(void) { ds_push(ds_pop()+1); }
static void w_1minus(void){ ds_push(ds_pop()-1); }
static void w_2star(void) { ds_push(ds_pop()<<1); }
static void w_2slash(void){ ds_push(ds_pop()>>1); }

/*=============================================================================
 * LOGIC / COMPARISON PRIMITIVES
 *=============================================================================*/
static void w_and(void)   { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a&b); }
static void w_or(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a|b); }
static void w_xor(void)   { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a^b); }
static void w_invert(void){ ds_push(~ds_pop()); }
static void w_lshift(void){ i64 n=ds_pop(); i64 v=ds_pop(); ds_push(v<<n); }
static void w_rshift(void){ i64 n=ds_pop(); i64 v=ds_pop(); ds_push((u64)v>>n); }
static void w_eq(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a==b?-1:0); }
static void w_neq(void)   { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a!=b?-1:0); }
static void w_lt(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a<b?-1:0); }
static void w_gt(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a>b?-1:0); }
static void w_le(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a<=b?-1:0); }
static void w_ge(void)    { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a>=b?-1:0); }
static void w_0eq(void)   { ds_push(ds_pop()==0?-1:0); }
static void w_0lt(void)   { ds_push(ds_pop()<0?-1:0); }
static void w_0gt(void)   { ds_push(ds_pop()>0?-1:0); }

/*=============================================================================
 * RETURN STACK PRIMITIVES
 *=============================================================================*/
static void w_to_r(void)  { rs_push((u64)ds_pop()); }
static void w_from_r(void){ ds_push((i64)rs_pop()); }
static void w_r_fetch(void){ ds_push((i64)rs_peek()); }

/*=============================================================================
 * MEMORY PRIMITIVES
 *=============================================================================*/
static void w_fetch(void)  { u64 *a=(u64*)ds_pop(); ds_push((i64)*a); }
static void w_store(void)  { u64 *a=(u64*)ds_pop(); i64 v=ds_pop(); *a=(u64)v; }
static void w_cfetch(void) { u8 *a=(u8*)ds_pop(); ds_push((i64)*a); }
static void w_cstore(void) { u8 *a=(u8*)ds_pop(); i64 v=ds_pop(); *a=(u8)v; }
static void w_plus_store(void){ u64 *a=(u64*)ds_pop(); i64 v=ds_pop(); *a+=v; }

/* HERE - push current dictionary pointer */
static void w_here(void) { ds_push((i64)(u64)dict_here); }

/* ALLOT - advance HERE by n bytes */
static void w_allot(void) { dict_here += ds_pop(); }

/* , (COMMA) - compile a cell at HERE */
static void w_comma(void) { dict_comma((cell_t)ds_pop()); }

/* C, - compile a byte at HERE */
static void w_c_comma(void) { dict_c_comma((u8)ds_pop()); }

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

static void w_dot_s(void) { ds_print(); }
static void w_emit(void)  { vga_putchar((char)ds_pop()); }
static void w_cr(void)    { vga_putchar('\n'); }
static void w_space(void) { vga_putchar(' '); }

static void w_spaces(void) {
    i64 n = ds_pop();
    for (i64 i = 0; i < n; i++) vga_putchar(' ');
}

static void w_type(void) {
    i64 len = ds_pop();
    char *addr = (char *)ds_pop();
    for (i64 i = 0; i < len; i++) vga_putchar(addr[i]);
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
    vga_put_hex((u64)ds_pop());
    vga_putchar(' ');
}

/*=============================================================================
 * COMPILER PRIMITIVES ( : ; IMMEDIATE )
 *=============================================================================*/

/* : (COLON) - Start compiling a new word */
static void w_colon(void) {
    char name[64];

    /* Parse the name of the new word */
    if (!parse_word(name, sizeof(name))) {
        vga_puts(" Missing word name after :\n");
        return;
    }

    /* Create the header with HIDDEN flag (unhide when ; is reached) */
    dict_create(name, F_HIDDEN);

    /* Set code field to DOCOL (colon definition handler) */
    dict_set_code(do_colon);

    /* Switch to compile state */
    forth_state = STATE_COMPILE;
}

/* ; (SEMICOLON) - End compilation, unhide word */
static void w_semicolon(void) {
    /* Compile EXIT sentinel */
    dict_comma(SENTINEL_EXIT);

    /* Unhide the word */
    u8 *entry = dict_latest;
    entry[8] &= ~F_HIDDEN;

    /* Return to interpret state */
    forth_state = STATE_INTERPRET;
}

/* IMMEDIATE - toggle immediate flag on most recent word */
static void w_immediate(void) {
    if (dict_latest) {
        dict_latest[8] ^= F_IMMEDIATE;
    }
}

/* ' (TICK) - Push the execution token (XT) of the next word */
static void w_tick(void) {
    char name[64];
    if (parse_word(name, sizeof(name))) {
        u8 *entry = dict_find(name);
        if (entry) {
            ds_push((i64)(u64)entry);
        } else {
            vga_puts(name);
            vga_puts(" ? Not found\n");
        }
    }
}

/* EXECUTE - execute XT on the stack */
static void w_execute(void) {
    u8 *entry = (u8 *)(u64)ds_pop();
    forth_execute(entry);
}

/* SEE - Decompile a word (show its definition) */
static void w_see(void) {
    char name[64];
    if (!parse_word(name, sizeof(name))) {
        vga_puts(" Usage: SEE wordname\n");
        return;
    }

    u8 *entry = dict_find(name);
    if (!entry) {
        vga_puts(name);
        vga_puts(" ? Not found\n");
        return;
    }

    cell_t *code = dict_get_code_field(entry);

    vga_puts(": ");
    /* Print name */
    u8 nlen = dict_get_name_len(entry);
    char *nname = dict_get_name(entry);
    for (u8 i = 0; i < nlen; i++) vga_putchar(nname[i]);
    vga_putchar(' ');

    if (*code != (cell_t)do_colon) {
        vga_puts("<native>\n");
        return;
    }

    /* Walk the parameter field */
    cell_t *pfa = dict_get_param_field(entry);
    while (*pfa != SENTINEL_EXIT) {
        if (*pfa == (u64)do_literal) {
            pfa++;
            char buf[24];
            ltoa((long)*pfa, buf, 10);
            vga_puts(buf);
            vga_putchar(' ');
        } else {
            u8 *w = (u8 *)*pfa;
            if ((u64)w >= DICT_VIRT_BASE &&
                (u64)w < DICT_VIRT_BASE + DICT_SIZE) {
                u8 wlen = dict_get_name_len(w);
                char *wname = dict_get_name(w);
                for (u8 i = 0; i < wlen; i++) vga_putchar(wname[i]);
                vga_putchar(' ');
            } else {
                vga_puts("??? ");
            }
        }
        pfa++;
    }
    vga_puts(";\n");
}

/*=============================================================================
 * CONTROL FLOW (IF/ELSE/THEN, DO/LOOP, BEGIN/UNTIL/AGAIN)
 * These work by compiling branch instructions into the parameter field.
 *=============================================================================*/

/* Runtime branch handlers */
static void rt_branch(void) {
    /* Unconditional branch: IP = *IP */
    u64 target = ip_fetch_advance();
    ip_set((u64 *)target);
}

static void rt_0branch(void) {
    /* Conditional branch: if TOS == 0, branch; else skip */
    u64 target = ip_fetch_advance();
    if (ds_pop() == 0) {
        ip_set((u64 *)target);
    }
}

/* Compile-time: IF ( -- addr ) */
static void w_if(void) {
    /* Compile a conditional branch with a placeholder target */
    dict_comma((cell_t)rt_0branch);
    ds_push((i64)(u64)dict_here);  /* Push addr of placeholder */
    dict_comma(0);                  /* Placeholder (will be patched by THEN) */
}

/* Compile-time: ELSE ( addr -- addr ) */
static void w_else(void) {
    /* Compile unconditional branch past the THEN block */
    dict_comma((cell_t)rt_branch);
    u64 else_placeholder = (u64)dict_here;
    dict_comma(0);  /* Placeholder for THEN */

    /* Patch the IF's placeholder to jump here */
    u64 if_placeholder = (u64)ds_pop();
    *(cell_t *)if_placeholder = (cell_t)(u64)dict_here;

    /* Push ELSE's placeholder for THEN to patch */
    ds_push((i64)else_placeholder);
}

/* Compile-time: THEN ( addr -- ) */
static void w_then(void) {
    /* Patch the placeholder from IF or ELSE */
    u64 placeholder = (u64)ds_pop();
    *(cell_t *)placeholder = (cell_t)(u64)dict_here;
}

/* Compile-time: BEGIN ( -- addr ) */
static void w_begin(void) {
    ds_push((i64)(u64)dict_here);  /* Push loop start address */
}

/* Compile-time: UNTIL ( addr -- ) */
static void w_until(void) {
    /* Compile conditional branch back to BEGIN */
    u64 begin_addr = (u64)ds_pop();
    dict_comma((cell_t)rt_0branch);
    dict_comma((cell_t)begin_addr);
}

/* Compile-time: AGAIN ( addr -- ) */
static void w_again(void) {
    /* Compile unconditional branch back to BEGIN */
    u64 begin_addr = (u64)ds_pop();
    dict_comma((cell_t)rt_branch);
    dict_comma((cell_t)begin_addr);
}

/* Compile-time: WHILE ( addr -- addr addr ) */
static void w_while(void) {
    /* Compile conditional branch with placeholder */
    dict_comma((cell_t)rt_0branch);
    ds_push((i64)(u64)dict_here);
    dict_comma(0);  /* Placeholder */
}

/* Compile-time: REPEAT ( addr addr -- ) */
static void w_repeat(void) {
    u64 while_placeholder = (u64)ds_pop();
    u64 begin_addr = (u64)ds_pop();

    /* Compile unconditional branch back to BEGIN */
    dict_comma((cell_t)rt_branch);
    dict_comma((cell_t)begin_addr);

    /* Patch WHILE's placeholder to jump here */
    *(cell_t *)while_placeholder = (cell_t)(u64)dict_here;
}

/* Runtime: DO handler */
static void rt_do(void) {
    i64 index = ds_pop();
    i64 limit = ds_pop();
    rs_push((u64)limit);
    rs_push((u64)index);
}

/* Runtime: LOOP handler */
static void rt_loop(void) {
    u64 target = ip_fetch_advance();
    i64 index = (i64)rs_pop();
    i64 limit = (i64)rs_peek();
    index++;
    if (index < limit) {
        rs_push((u64)index);
        ip_set((u64 *)target);
    } else {
        rs_pop(); /* Remove limit */
    }
}

/* Runtime: I - push loop index */
static void w_i(void) {
    ds_push((i64)rs_peek());
}

/* Compile-time: DO ( -- addr ) */
static void w_do(void) {
    dict_comma((cell_t)rt_do);
    ds_push((i64)(u64)dict_here);  /* Push loop body address */
}

/* Compile-time: LOOP ( addr -- ) */
static void w_loop(void) {
    u64 do_addr = (u64)ds_pop();
    dict_comma((cell_t)rt_loop);
    dict_comma((cell_t)do_addr);
}

/*=============================================================================
 * SYSTEM / UTILITY PRIMITIVES
 *=============================================================================*/
static void w_words(void)  { dict_list_words(); }

static void w_bye(void) {
    vga_puts("\nSystem halted.\n");
    cli(); while (1) hlt();
}

static void w_clear(void)   { vga_clear(); }
static void w_decimal(void) { forth_base = 10; }
static void w_hex(void)     { forth_base = 16; }
static void w_base(void)    { ds_push((i64)forth_base); }

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
    extern void kernel_main(u64, e820_entry_t *);

    vga_puts("=== System Status ===\n");
    vga_puts("kernel_main: "); vga_put_hex((u64)&kernel_main); vga_putchar('\n');
    vga_puts("Ticks:       "); ultoa(kernel_ticks, buf, 10); vga_puts(buf); vga_putchar('\n');
    vga_puts("Heap total:  "); ultoa(heap_get_total(), buf, 10); vga_puts(buf); vga_puts(" B\n");
    vga_puts("Heap used:   "); ultoa(heap_get_used(), buf, 10); vga_puts(buf); vga_puts(" B\n");
    vga_puts("Heap free:   "); ultoa(heap_get_free(), buf, 10); vga_puts(buf); vga_puts(" B\n");
    vga_puts("PMM free:    "); ultoa(pmm_get_free_memory()/1024, buf, 10); vga_puts(buf); vga_puts(" KB\n");
    vga_puts("Dict HERE:   "); vga_put_hex((u64)dict_here); vga_putchar('\n');
    vga_puts("Dict used:   "); ultoa((u64)(dict_here - dict_base), buf, 10); vga_puts(buf); vga_puts(" B\n");
    vga_puts("Stack depth: "); itoa((int)ds_depth(), buf, 10); vga_puts(buf); vga_putchar('\n');
    vga_puts("Base:        "); itoa(forth_base, buf, 10); vga_puts(buf); vga_putchar('\n');
    vga_puts("State:       "); vga_puts(forth_state ? "COMPILE" : "INTERPRET"); vga_putchar('\n');
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
    vga_puts("Stack:    DUP DROP SWAP OVER ROT NIP TUCK .S DEPTH\n");
    vga_puts("Math:     + - * / MOD NEGATE ABS MIN MAX 1+ 1-\n");
    vga_puts("Logic:    AND OR XOR INVERT LSHIFT RSHIFT\n");
    vga_puts("Compare:  = <> < > <= >= 0= 0< 0>\n");
    vga_puts("Memory:   @ ! C@ C! +! HERE ALLOT , C,\n");
    vga_puts("Output:   . U. .S .HEX EMIT CR SPACE SPACES TYPE .\"\n");
    vga_puts("Return:   >R R> R@\n");
    vga_puts("Compiler: : ; IMMEDIATE ' EXECUTE SEE\n");
    vga_puts("Control:  IF ELSE THEN BEGIN UNTIL AGAIN WHILE REPEAT\n");
    vga_puts("Loops:    DO LOOP I\n");
    vga_puts("System:   WORDS HELP STATUS FREE TICKS DUMP\n");
    vga_puts("Base:     DECIMAL HEX BASE\n");
    vga_puts("Other:    CLEAR BYE\n");
    vga_puts("\nExamples:\n");
    vga_puts("  10 20 + .               \\ prints 30\n");
    vga_puts("  : SQUARE DUP * ;        \\ define new word\n");
    vga_puts("  5 SQUARE .              \\ prints 25\n");
    vga_puts("  : FACT 1 SWAP 1+ 1 DO I * LOOP ; \n");
    vga_puts("  10 FACT .               \\ prints 3628800\n");
    vga_puts("  SEE SQUARE              \\ decompile\n");
}

/*=============================================================================
 * Register All Primitives
 *=============================================================================*/
static void register_primitives(void) {
    /* Stack */
    dict_add_primitive("DUP",    w_dup,    0);
    dict_add_primitive("DROP",   w_drop,   0);
    dict_add_primitive("SWAP",   w_swap,   0);
    dict_add_primitive("OVER",   w_over,   0);
    dict_add_primitive("ROT",    w_rot,    0);
    dict_add_primitive("NIP",    w_nip,    0);
    dict_add_primitive("TUCK",   w_tuck,   0);
    dict_add_primitive("2DUP",   w_2dup,   0);
    dict_add_primitive("2DROP",  w_2drop,  0);
    dict_add_primitive("2SWAP",  w_2swap,  0);
    dict_add_primitive("?DUP",   w_qdup,   0);
    dict_add_primitive("DEPTH",  w_depth,  0);

    /* Arithmetic */
    dict_add_primitive("+",      w_plus,   0);
    dict_add_primitive("-",      w_minus,  0);
    dict_add_primitive("*",      w_mul,    0);
    dict_add_primitive("/",      w_div,    0);
    dict_add_primitive("MOD",    w_mod,    0);
    dict_add_primitive("/MOD",   w_divmod, 0);
    dict_add_primitive("NEGATE", w_negate, 0);
    dict_add_primitive("ABS",    w_abs,    0);
    dict_add_primitive("MIN",    w_min,    0);
    dict_add_primitive("MAX",    w_max,    0);
    dict_add_primitive("1+",     w_1plus,  0);
    dict_add_primitive("1-",     w_1minus, 0);
    dict_add_primitive("2*",     w_2star,  0);
    dict_add_primitive("2/",     w_2slash, 0);

    /* Logic & Comparison */
    dict_add_primitive("AND",    w_and,    0);
    dict_add_primitive("OR",     w_or,     0);
    dict_add_primitive("XOR",    w_xor,    0);
    dict_add_primitive("INVERT", w_invert, 0);
    dict_add_primitive("LSHIFT", w_lshift, 0);
    dict_add_primitive("RSHIFT", w_rshift, 0);
    dict_add_primitive("=",      w_eq,     0);
    dict_add_primitive("<>",     w_neq,    0);
    dict_add_primitive("<",      w_lt,     0);
    dict_add_primitive(">",      w_gt,     0);
    dict_add_primitive("<=",     w_le,     0);
    dict_add_primitive(">=",     w_ge,     0);
    dict_add_primitive("0=",     w_0eq,    0);
    dict_add_primitive("0<",     w_0lt,    0);
    dict_add_primitive("0>",     w_0gt,    0);

    /* Return Stack */
    dict_add_primitive(">R",     w_to_r,   0);
    dict_add_primitive("R>",     w_from_r, 0);
    dict_add_primitive("R@",     w_r_fetch,0);

    /* Memory */
    dict_add_primitive("@",      w_fetch,  0);
    dict_add_primitive("!",      w_store,  0);
    dict_add_primitive("C@",     w_cfetch, 0);
    dict_add_primitive("C!",     w_cstore, 0);
    dict_add_primitive("+!",     w_plus_store, 0);
    dict_add_primitive("HERE",   w_here,   0);
    dict_add_primitive("ALLOT",  w_allot,  0);
    dict_add_primitive(",",      w_comma,  0);
    dict_add_primitive("C,",     w_c_comma,0);

    /* Output */
    dict_add_primitive(".",      w_dot,    0);
    dict_add_primitive("U.",     w_u_dot,  0);
    dict_add_primitive(".S",     w_dot_s,  0);
    dict_add_primitive(".HEX",   w_dot_hex,0);
    dict_add_primitive("EMIT",   w_emit,   0);
    dict_add_primitive("CR",     w_cr,     0);
    dict_add_primitive("SPACE",  w_space,  0);
    dict_add_primitive("SPACES", w_spaces, 0);
    dict_add_primitive("TYPE",   w_type,   0);
    dict_add_primitive(".\"",    w_dot_quote, F_IMMEDIATE);
    dict_add_primitive("(",      w_paren,  F_IMMEDIATE);

    /* Compiler */
    dict_add_primitive(":",      w_colon,  0);
    dict_add_primitive(";",      w_semicolon, F_IMMEDIATE);
    dict_add_primitive("IMMEDIATE", w_immediate, 0);
    dict_add_primitive("'",      w_tick,   0);
    dict_add_primitive("EXECUTE",w_execute,0);
    dict_add_primitive("SEE",    w_see,    0);

    /* Control Flow (compile-only, immediate) */
    dict_add_primitive("IF",     w_if,     F_IMMEDIATE);
    dict_add_primitive("ELSE",   w_else,   F_IMMEDIATE);
    dict_add_primitive("THEN",   w_then,   F_IMMEDIATE);
    dict_add_primitive("BEGIN",  w_begin,  F_IMMEDIATE);
    dict_add_primitive("UNTIL",  w_until,  F_IMMEDIATE);
    dict_add_primitive("AGAIN",  w_again,  F_IMMEDIATE);
    dict_add_primitive("WHILE",  w_while,  F_IMMEDIATE);
    dict_add_primitive("REPEAT", w_repeat, F_IMMEDIATE);
    dict_add_primitive("DO",     w_do,     F_IMMEDIATE);
    dict_add_primitive("LOOP",   w_loop,   F_IMMEDIATE);
    dict_add_primitive("I",      w_i,      0);

    /* System */
    dict_add_primitive("WORDS",  w_words,  0);
    dict_add_primitive("HELP",   w_help,   0);
    dict_add_primitive("STATUS", w_status, 0);
    dict_add_primitive("FREE",   w_free,   0);
    dict_add_primitive("TICKS",  w_ticks,  0);
    dict_add_primitive("DUMP",   w_dump,   0);
    dict_add_primitive("CLEAR",  w_clear,  0);
    dict_add_primitive("BYE",    w_bye,    0);
    dict_add_primitive("BASE",   w_base,   0);
    dict_add_primitive("DECIMAL",w_decimal,0);
    dict_add_primitive("HEX",    w_hex,    0);
}

/*=============================================================================
 * Outer Interpreter
 *=============================================================================*/
void forth_eval(const char *line) {
    char token[64];
    parse_ptr = line;

    while (parse_word(token, sizeof(token))) {
        /* 1. Search dictionary */
        u8 *entry = dict_find(token);

        if (entry) {
            u8 flags = dict_get_flags(entry);

            if (forth_state == STATE_COMPILE && !(flags & F_IMMEDIATE)) {
                /* COMPILE MODE: compile a reference to this word */
                dict_comma((cell_t)(u64)entry);
            } else {
                /* INTERPRET MODE (or IMMEDIATE word): execute now */
                forth_execute(entry);
            }
            continue;
        }

        /* 2. Try to parse as a number */
        i64 num;
        if (try_parse_number(token, &num)) {
            if (forth_state == STATE_COMPILE) {
                /* Compile a literal: [do_literal] [value] */
                dict_comma((cell_t)do_literal);
                dict_comma((cell_t)num);
            } else {
                ds_push(num);
            }
            continue;
        }

        /* 3. Unknown word */
        vga_puts(token);
        vga_puts(" ? Unknown word\n");
        forth_state = STATE_INTERPRET;  /* Reset state on error */
        return;
    }

    /* Only print "ok" in interpret mode */
    if (forth_state == STATE_INTERPRET) {
        vga_puts(" ok\n");
    }
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
    vga_puts("  Threaded-Code Compiler Ready\n");
    vga_puts("  Type HELP for commands\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    serial_puts(SERIAL_COM1, "Forth VM initialized (threaded code)\n");
}

/*=============================================================================
 * REPL
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
                if (forth_state == STATE_COMPILE) {
                    vga_puts("| ");
                } else {
                    vga_puts("> ");
                }
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
            else if (c == '\b') {
                if (input_pos > 0) input_pos--;
            }
            else if (c >= 32 && input_pos < INPUT_BUF_SIZE - 1) {
                input_buffer[input_pos++] = c;
            }
        }
        hlt();
    }
}