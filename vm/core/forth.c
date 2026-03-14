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
#include "../../kernel/scheduler/task.h"
#include "../../fs/block_device.h"
#include "../../fs/filesystem.h"

/*=============================================================================
 * Interpreter State
 *=============================================================================*/
static int forth_state = STATE_INTERPRET;
static int forth_base  = 10;

int  forth_get_state(void) { return forth_state; }
void forth_set_state(int s) { forth_state = s; }
int  forth_get_base(void)  { return forth_base; }
void forth_set_base(int b) { forth_base = b; }

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
    if (word[0] == '0' && (word[1] == 'x' || word[1] == 'X')) { base = 16; word += 2; }
    else if (word[0] == '$') { base = 16; word++; }
    else if (word[0] == '#') { base = 10; word++; }
    else if (word[0] == '%') { base = 2; word++; }
    long val = strtol(word, &end, base);
    if (*end == '\0' && end != word) { *out = (i64)val; return 1; }
    return 0;
}

/*=============================================================================
 * INNER INTERPRETER
 *=============================================================================*/
#define SENTINEL_EXIT 0xDEADC0DE00000000ULL

static void forth_inner(void);

void do_colon(void) {}
void do_literal(void) { ds_push((i64)ip_fetch_advance()); }
void do_variable(void) {}
void do_constant(void) {}

void forth_execute(u8 *entry) {
    cell_t *code_field = dict_get_code_field(entry);
    cell_t code = *code_field;

    if (code == (cell_t)do_colon) {
        rs_push((u64)ip_get());
        ip_set((u64 *)dict_get_param_field(entry));
        forth_inner();
    } else if (code == (cell_t)do_variable) {
        ds_push((i64)(u64)dict_get_param_field(entry));
    } else if (code == (cell_t)do_constant) {
        ds_push((i64)*dict_get_param_field(entry));
    } else {
        native_fn_t fn = (native_fn_t)code;
        fn();
    }
}

static void forth_inner(void) {
    while (1) {
        u64 cell = ip_fetch_advance();

        if (cell == SENTINEL_EXIT) {
            u64 saved_ip = rs_pop();
            ip_set((u64 *)saved_ip);
            if (saved_ip == 0) return;
            continue;
        }

        if (cell == (u64)do_literal) {
            ds_push((i64)ip_fetch_advance());
            continue;
        }

        u8 *entry = (u8 *)cell;
        if ((u64)entry < DICT_VIRT_BASE || (u64)entry >= DICT_VIRT_BASE + DICT_SIZE) {
            vga_puts(" Invalid word address!\n");
            return;
        }

        cell_t *code_field = dict_get_code_field(entry);
        cell_t code = *code_field;

        if (code == (cell_t)do_colon) {
            rs_push((u64)ip_get());
            ip_set((u64 *)dict_get_param_field(entry));
        } else if (code == (cell_t)do_variable) {
            ds_push((i64)(u64)dict_get_param_field(entry));
        } else if (code == (cell_t)do_constant) {
            ds_push((i64)*dict_get_param_field(entry));
        } else {
            native_fn_t fn = (native_fn_t)code;
            fn();
        }
    }
}

/*=============================================================================
 * SIMD VECTOR PRIMITIVES
 *=============================================================================*/

/* VDUP: Uses SSE2 to copy 1 value into 2 stack slots instantly */
__attribute__((target("sse2")))
static void w_vdup(void) {
    if (ds_depth() < 1) return;
    i64 top = ds_pop();
    
    /* 16-byte aligned memory for 128-bit XMM spill */
    __attribute__((aligned(16))) i64 vec_out[2];

    __asm__ volatile (
        "movq %1, %%xmm0\n\t"         /* Move 64-bit value to XMM0 */
        "punpcklqdq %%xmm0, %%xmm0\n\t" /* Unpack/duplicate it into both halves */
        "movdqa %%xmm0, %0\n\t"       /* Spill 128-bits back to RAM */
        : "=m" (vec_out)
        : "r" (top)
        : "xmm0", "memory"
    );

    ds_push(vec_out[0]);
    ds_push(vec_out[1]);
}

/* V+: Pops 4 items, does 2 simultaneous additions in 1 cycle, pushes 2 results */
__attribute__((target("sse2")))
static void w_vplus(void) {
    if (ds_depth() < 4) { vga_puts(" Stack Underflow for V+\n"); return; }
    
    __attribute__((aligned(16))) i64 v_a[2];
    __attribute__((aligned(16))) i64 v_b[2];
    
    v_b[1] = ds_pop(); v_b[0] = ds_pop();
    v_a[1] = ds_pop(); v_a[0] = ds_pop();

    __asm__ volatile (
        "movdqa %1, %%xmm1\n\t"       /* Load first vector */
        "movdqa %2, %%xmm2\n\t"       /* Load second vector */
        "paddq %%xmm2, %%xmm1\n\t"    /* Add them simultaneously */
        "movdqa %%xmm1, %0\n\t"       /* Spill to memory */
        : "=m" (v_a)
        : "m" (v_a), "m" (v_b)
        : "xmm1", "xmm2", "memory"
    );

    ds_push(v_a[0]);
    ds_push(v_a[1]);
}

/*=============================================================================
 * PRIMITIVES
 *=============================================================================*/
static void w_dup(void)  { ds_push(ds_peek()); }
static void w_drop(void) { ds_pop(); }
static void w_swap(void) { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(b); ds_push(a); }
static void w_over(void) { ds_push(ds_pick(1)); }
static void w_rot(void)  { i64 c=ds_pop(); i64 b=ds_pop(); i64 a=ds_pop(); ds_push(b); ds_push(c); ds_push(a); }
static void w_nip(void)  { i64 t=ds_pop(); ds_pop(); ds_push(t); }
static void w_tuck(void) { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(b); ds_push(a); ds_push(b); }
static void w_2dup(void) { i64 b=ds_peek(); i64 a=ds_pick(1); ds_push(a); ds_push(b); }
static void w_2drop(void){ ds_pop(); ds_pop(); }
static void w_2swap(void){ i64 d=ds_pop(); i64 c=ds_pop(); i64 b=ds_pop(); i64 a=ds_pop(); ds_push(c); ds_push(d); ds_push(a); ds_push(b); }
static void w_qdup(void) { i64 t=ds_peek(); if(t) ds_push(t); }
static void w_depth(void){ ds_push(ds_depth()); }

static void w_plus(void) { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a+b); }
static void w_minus(void){ i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a-b); }
static void w_mul(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a*b); }
static void w_div(void)  { i64 b=ds_pop(); if(!b){vga_puts(" Div/0!\n");ds_push(0);return;} i64 a=ds_pop(); ds_push(a/b); }
static void w_mod(void)  { i64 b=ds_pop(); if(!b){vga_puts(" Div/0!\n");ds_push(0);return;} i64 a=ds_pop(); ds_push(a%b); }
static void w_divmod(void){ i64 b=ds_pop(); if(!b){vga_puts(" Div/0!\n");ds_push(0);ds_push(0);return;} i64 a=ds_pop(); ds_push(a%b); ds_push(a/b); }
static void w_negate(void){ ds_push(-ds_pop()); }
static void w_abs(void)  { i64 v=ds_pop(); ds_push(v<0?-v:v); }
static void w_min(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a<b?a:b); }
static void w_max(void)  { i64 b=ds_pop(); i64 a=ds_pop(); ds_push(a>b?a:b); }
static void w_1plus(void) { ds_push(ds_pop()+1); }
static void w_1minus(void){ ds_push(ds_pop()-1); }
static void w_2star(void) { ds_push(ds_pop()<<1); }
static void w_2slash(void){ ds_push(ds_pop()>>1); }

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

static void w_to_r(void)  { rs_push((u64)ds_pop()); }
static void w_from_r(void){ ds_push((i64)rs_pop()); }
static void w_r_fetch(void){ ds_push((i64)rs_peek()); }

static void w_fetch(void)  { u64 *a=(u64*)ds_pop(); ds_push((i64)*a); }
static void w_store(void)  { u64 *a=(u64*)ds_pop(); i64 v=ds_pop(); *a=(u64)v; }
static void w_cfetch(void) { u8 *a=(u8*)ds_pop(); ds_push((i64)*a); }
static void w_cstore(void) { u8 *a=(u8*)ds_pop(); i64 v=ds_pop(); *a=(u8)v; }
static void w_plus_store(void){ u64 *a=(u64*)ds_pop(); i64 v=ds_pop(); *a+=v; }

static void w_here(void) { ds_push((i64)(u64)dict_here); }
static void w_allot(void) { dict_here += ds_pop(); }
static void w_comma(void) { dict_comma((cell_t)ds_pop()); }
static void w_c_comma(void) { dict_c_comma((u8)ds_pop()); }

static void w_dot(void) { char buf[24]; ltoa((long)ds_pop(), buf, forth_base); vga_puts(buf); vga_putchar(' '); }
static void w_u_dot(void) { char buf[24]; ultoa((u64)ds_pop(), buf, forth_base); vga_puts(buf); vga_putchar(' '); }
static void w_dot_s(void) { ds_print(); }
static void w_emit(void)  { vga_putchar((char)ds_pop()); }
static void w_cr(void)    { vga_putchar('\n'); }
static void w_space(void) { vga_putchar(' '); }
static void w_spaces(void) { i64 n=ds_pop(); for(i64 i=0;i<n;i++) vga_putchar(' '); }
static void w_type(void) { i64 len=ds_pop(); char *a=(char*)ds_pop(); for(i64 i=0;i<len;i++) vga_putchar(a[i]); }
static void w_dot_quote(void) { char buf[128]; parse_until('"', buf, sizeof(buf)); vga_puts(buf); }
static void w_paren(void) { char buf[256]; parse_until(')', buf, sizeof(buf)); }
static void w_dot_hex(void) { vga_put_hex((u64)ds_pop()); vga_putchar(' '); }

/*=============================================================================
 * COMPILER & DATA STRUCTURES
 *=============================================================================*/
static void w_colon(void) {
    char name[64];
    if (!parse_word(name, sizeof(name))) return;
    dict_create(name, F_HIDDEN);
    dict_set_code(do_colon);
    forth_state = STATE_COMPILE;
}

static void w_semicolon(void) {
    dict_comma(SENTINEL_EXIT);
    dict_latest[8] &= ~F_HIDDEN;
    forth_state = STATE_INTERPRET;
}

static void w_create(void) {
    char name[64];
    if (!parse_word(name, sizeof(name))) return;
    dict_create(name, 0);
    dict_set_code(do_variable);
}

static void w_variable(void) {
    w_create();
    dict_comma(0); 
}

static void w_constant(void) {
    char name[64];
    if (!parse_word(name, sizeof(name))) return;
    dict_create(name, 0);
    dict_set_code(do_constant);
    dict_comma((cell_t)ds_pop());
}

static void w_immediate(void) { if (dict_latest) dict_latest[8] ^= F_IMMEDIATE; }

static void w_tick(void) {
    char name[64];
    if (parse_word(name, sizeof(name))) {
        u8 *entry = dict_find(name);
        if (entry) ds_push((i64)(u64)entry);
        else { vga_puts(name); vga_puts(" ? Not found\n"); }
    }
}

static void w_execute(void) { forth_execute((u8 *)(u64)ds_pop()); }

static void w_see(void) {
    char name[64];
    if (!parse_word(name, sizeof(name))) return;
    u8 *entry = dict_find(name);
    if (!entry) { vga_puts(" ? Not found\n"); return; }

    cell_t *code = dict_get_code_field(entry);
    vga_puts(": ");
    u8 nlen = dict_get_name_len(entry); 
    char *nname = dict_get_name(entry);
    for (u8 i = 0; i < nlen; i++) {
        vga_putchar(nname[i]);
    }
    vga_putchar(' ');

    if (*code == (cell_t)do_variable) { vga_puts("VARIABLE\n"); return; }
    if (*code == (cell_t)do_constant) { vga_puts("CONSTANT\n"); return; }
    if (*code != (cell_t)do_colon)    { vga_puts("<native>\n"); return; }

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
            if ((u64)w >= DICT_VIRT_BASE && (u64)w < DICT_VIRT_BASE + DICT_SIZE) {
                u8 wlen = dict_get_name_len(w); 
                char *wname = dict_get_name(w);
                for (u8 i = 0; i < wlen; i++) {
                    vga_putchar(wname[i]);
                }
                vga_putchar(' ');
            } else {
                vga_puts("??? ");
            }
        }
        pfa++;
    }
    vga_puts(";\n");
}

static void w_load(void) {
    u32 blk_num = (u32)ds_pop();
    block_t *blk = block_get(blk_num);
    
    /* Save the current parser state so we can return to the terminal 
     * after the disk file finishes executing */
    const char *saved_ptr = parse_ptr;
    
    /* Copy the 1008-byte block payload into a safe string buffer */
    char buf[1024];
    for (int i = 0; i < 1008; i++) {
        buf[i] = blk->data[i];
        /* Treat uninitialized block data as spaces */
        if (buf[i] < 32 || buf[i] > 126) buf[i] = ' '; 
    }
    buf[1008] = '\0'; /* Null terminate it */

    /* Tell the VM to evaluate the disk buffer! */
    vga_puts("\n[Loading Block ");
    char nbuf[10]; itoa(blk_num, nbuf, 10); vga_puts(nbuf);
    vga_puts("...]\n");
    
    forth_eval(buf);
    
    /* Restore parser to the keyboard terminal */
    parse_ptr = saved_ptr;
}

/* Control Flow */
static void rt_branch(void)  { ip_set((u64 *)ip_fetch_advance()); }
static void rt_0branch(void) { u64 t = ip_fetch_advance(); if (ds_pop() == 0) ip_set((u64 *)t); }

static void w_if(void)     { dict_comma((cell_t)rt_0branch); ds_push((i64)(u64)dict_here); dict_comma(0); }
static void w_else(void)   { dict_comma((cell_t)rt_branch); u64 e = (u64)dict_here; dict_comma(0);
                             *(cell_t *)ds_pop() = (cell_t)(u64)dict_here; ds_push((i64)e); }
static void w_then(void)   { *(cell_t *)ds_pop() = (cell_t)(u64)dict_here; }
static void w_begin(void)  { ds_push((i64)(u64)dict_here); }
static void w_until(void)  { u64 b = (u64)ds_pop(); dict_comma((cell_t)rt_0branch); dict_comma(b); }
static void w_again(void)  { u64 b = (u64)ds_pop(); dict_comma((cell_t)rt_branch); dict_comma(b); }
static void w_while(void)  { dict_comma((cell_t)rt_0branch); ds_push((i64)(u64)dict_here); dict_comma(0); }
static void w_repeat(void) { u64 w = (u64)ds_pop(); u64 b = (u64)ds_pop(); dict_comma((cell_t)rt_branch); dict_comma(b); *(cell_t *)w = (cell_t)(u64)dict_here; }

static void rt_do(void)    { i64 i=ds_pop(); rs_push((u64)ds_pop()); rs_push((u64)i); }
static void rt_loop(void)  { u64 t=ip_fetch_advance(); i64 i=(i64)rs_pop(); i64 l=(i64)rs_peek();
                             if(++i < l) { rs_push((u64)i); ip_set((u64*)t); } else rs_pop(); }
static void w_i(void)      { ds_push((i64)rs_peek()); }
static void w_do(void)     { dict_comma((cell_t)rt_do); ds_push((i64)(u64)dict_here); }
static void w_loop(void)   { u64 d = (u64)ds_pop(); dict_comma((cell_t)rt_loop); dict_comma(d); }

/*=============================================================================
 * SYSTEM / UTILITY PRIMITIVES
 *=============================================================================*/
static u8 *fork_target_xt = NULL;

static void forth_thread_wrapper(void) {
    if (fork_target_xt) {
        forth_execute(fork_target_xt);
    }
}

static void w_fork(void) {
    u8 *xt = (u8 *)(u64)ds_pop();
    fork_target_xt = xt;
    task_t *new_task = task_create(forth_thread_wrapper);
    ds_push(new_task->pid);
}
/*=============================================================================
 * FILESYSTEM (BLOCK) PRIMITIVES
 *=============================================================================*/
static void w_block(void) {
    u32 blk_num = (u32)ds_pop();
    block_t *blk = block_get(blk_num);
    
    /* Push the address of the 1008-byte DATA section to the stack, 
     * skipping over the 16-byte header so the user can write freely. */
    ds_push((i64)(u64)blk->data);
}

static void w_update(void) {
    block_update();
}

static void w_flush(void) {
    block_flush();
}

static void w_words(void)  { dict_list_words(); }
static void w_bye(void)    { vga_puts("\nSystem halted.\n"); cli(); while (1) hlt(); }
static void w_clear(void)  { vga_clear(); }
static void w_decimal(void){ forth_base = 10; }
static void w_hex(void)    { forth_base = 16; }
static void w_base(void)   { ds_push((i64)forth_base); }

static void w_status(void) {
    char buf[24];
    extern volatile u64 kernel_ticks;
    extern void kernel_main(u64, e820_entry_t *);

    vga_puts("=== System Status ===\n");
    vga_puts("Ticks:       "); ultoa(kernel_ticks, buf, 10); vga_puts(buf); vga_putchar('\n');
    vga_puts("Dict HERE:   "); vga_put_hex((u64)dict_here); vga_putchar('\n');
    vga_puts("Stack depth: "); itoa((int)ds_depth(), buf, 10); vga_puts(buf); vga_putchar('\n');
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



/*=============================================================================
 * VISUAL BLOCK EDITOR
 * Screen is 15 lines of 64 characters (960 bytes)
 *=============================================================================*/
static void run_editor(u32 block_num) {
    block_t *blk = block_get(block_num);
    int cursor = 0;

    while (1) {
        /* Draw the Editor UI */
        vga_clear();
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        vga_puts("=== VECTOR FORTH BLOCK EDITOR === [Block ");
        char bbuf[10]; itoa(block_num, bbuf, 10); vga_puts(bbuf);
        vga_puts("] === (Press ESC to Save/Exit) ===\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

        /* Draw 15 lines of 64 characters */
        for (int i = 0; i < 960; i++) {
            if (i % 64 == 0 && i != 0) vga_putchar('\n');
            
            /* Highlight the cursor position */
            if (i == cursor) vga_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
            
            char c = blk->data[i];
            if (c < 32 || c > 126) c = '.'; /* Draw dots for empty space */
            vga_putchar(c);
            
            if (i == cursor) vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        }

        /* Handle Input */
        while (!keyboard_available()) yield();
        char key = keyboard_getchar();

        if (key == 27) { /* ESC key */
            break;
        } else if (key == '\b') {
            if (cursor > 0) { cursor--; blk->data[cursor] = 0; }
        } else if (key == '\n') {
            /* Move cursor to next line */
            cursor = (cursor / 64 + 1) * 64;
            if (cursor >= 960) cursor = 959;
        } else if (key >= 32 && key <= 126) {
            blk->data[cursor] = key;
            if (cursor < 959) cursor++;
        }
    }

    block_update();
    block_flush();
    vga_clear();
}

static void w_edit(void) {
    u32 blk = (u32)ds_pop();
    run_editor(blk);
}

static void register_primitives(void) {
    dict_add_primitive("DUP", w_dup, 0); dict_add_primitive("DROP", w_drop, 0);
    dict_add_primitive("SWAP", w_swap, 0); dict_add_primitive("OVER", w_over, 0);
    dict_add_primitive("ROT", w_rot, 0); dict_add_primitive("NIP", w_nip, 0);
    dict_add_primitive("TUCK", w_tuck, 0); dict_add_primitive("2DUP", w_2dup, 0);
    dict_add_primitive("2DROP", w_2drop, 0); dict_add_primitive("2SWAP", w_2swap, 0);
    dict_add_primitive("?DUP", w_qdup, 0); dict_add_primitive("DEPTH", w_depth, 0);
    dict_add_primitive("+", w_plus, 0); dict_add_primitive("-", w_minus, 0);
    dict_add_primitive("*", w_mul, 0); dict_add_primitive("/", w_div, 0);
    dict_add_primitive("MOD", w_mod, 0); dict_add_primitive("/MOD", w_divmod, 0);
    dict_add_primitive("NEGATE", w_negate, 0); dict_add_primitive("ABS", w_abs, 0);
    dict_add_primitive("MIN", w_min, 0); dict_add_primitive("MAX", w_max, 0);
    dict_add_primitive("1+", w_1plus, 0); dict_add_primitive("1-", w_1minus, 0);
    dict_add_primitive("2*", w_2star, 0); dict_add_primitive("2/", w_2slash, 0);
    dict_add_primitive("AND", w_and, 0); dict_add_primitive("OR", w_or, 0);
    dict_add_primitive("XOR", w_xor, 0); dict_add_primitive("INVERT", w_invert, 0);
    dict_add_primitive("LSHIFT", w_lshift, 0); dict_add_primitive("RSHIFT", w_rshift, 0);
    dict_add_primitive("=", w_eq, 0); dict_add_primitive("<>", w_neq, 0);
    dict_add_primitive("<", w_lt, 0); dict_add_primitive(">", w_gt, 0);
    dict_add_primitive("<=", w_le, 0); dict_add_primitive(">=", w_ge, 0);
    dict_add_primitive("0=", w_0eq, 0); dict_add_primitive("0<", w_0lt, 0); dict_add_primitive("0>", w_0gt, 0);
    dict_add_primitive(">R", w_to_r, 0); dict_add_primitive("R>", w_from_r, 0); dict_add_primitive("R@", w_r_fetch, 0);
    dict_add_primitive("@", w_fetch, 0); dict_add_primitive("!", w_store, 0);
    dict_add_primitive("C@", w_cfetch, 0); dict_add_primitive("C!", w_cstore, 0); dict_add_primitive("+!", w_plus_store, 0);
    dict_add_primitive("HERE", w_here, 0); dict_add_primitive("ALLOT", w_allot, 0);
    dict_add_primitive(",", w_comma, 0); dict_add_primitive("C,", w_c_comma, 0);
    dict_add_primitive(".", w_dot, 0); dict_add_primitive("U.", w_u_dot, 0);
    dict_add_primitive(".S", w_dot_s, 0); dict_add_primitive(".HEX", w_dot_hex, 0);
    dict_add_primitive("EMIT", w_emit, 0); dict_add_primitive("CR", w_cr, 0);
    dict_add_primitive("SPACE", w_space, 0); dict_add_primitive("SPACES", w_spaces, 0);
    dict_add_primitive("TYPE", w_type, 0); dict_add_primitive(".\"", w_dot_quote, F_IMMEDIATE);
    dict_add_primitive("(", w_paren, F_IMMEDIATE);
    dict_add_primitive(":", w_colon, 0); dict_add_primitive(";", w_semicolon, F_IMMEDIATE);
    
    dict_add_primitive("VARIABLE", w_variable, 0);
    dict_add_primitive("CONSTANT", w_constant, 0);
    dict_add_primitive("CREATE", w_create, 0);
    
    dict_add_primitive("IMMEDIATE", w_immediate, 0); dict_add_primitive("'", w_tick, 0);
    dict_add_primitive("EXECUTE", w_execute, 0); dict_add_primitive("SEE", w_see, 0);
    dict_add_primitive("IF", w_if, F_IMMEDIATE); dict_add_primitive("ELSE", w_else, F_IMMEDIATE);
    dict_add_primitive("THEN", w_then, F_IMMEDIATE); dict_add_primitive("BEGIN", w_begin, F_IMMEDIATE);
    dict_add_primitive("UNTIL", w_until, F_IMMEDIATE); dict_add_primitive("AGAIN", w_again, F_IMMEDIATE);
    dict_add_primitive("WHILE", w_while, F_IMMEDIATE); dict_add_primitive("REPEAT", w_repeat, F_IMMEDIATE);
    dict_add_primitive("DO", w_do, F_IMMEDIATE); dict_add_primitive("LOOP", w_loop, F_IMMEDIATE);
    dict_add_primitive("I", w_i, 0); dict_add_primitive("WORDS", w_words, 0);
    dict_add_primitive("STATUS", w_status, 0);
    dict_add_primitive("DUMP", w_dump, 0);
    dict_add_primitive("FORK", w_fork, 0);
    dict_add_primitive("YIELD", yield, 0);
    dict_add_primitive("CLEAR", w_clear, 0); dict_add_primitive("BYE", w_bye, 0);
    dict_add_primitive("BASE", w_base, 0); dict_add_primitive("DECIMAL", w_decimal, 0);
    dict_add_primitive("HEX", w_hex, 0);
        /* Filesystem */
    dict_add_primitive("BLOCK",  w_block,  0);
    dict_add_primitive("UPDATE", w_update, 0);
    dict_add_primitive("FLUSH",  w_flush,  0);

    dict_add_primitive("EDIT",   w_edit,   0);

    dict_add_primitive("VDUP", w_vdup, 0);
    dict_add_primitive("V+",   w_vplus, 0);
    dict_add_primitive("LOAD", w_load, 0);
}

void forth_eval(const char *line) {
    char token[64];
    parse_ptr = line;
    while (parse_word(token, sizeof(token))) {
        u8 *entry = dict_find(token);
        if (entry) {
            u8 flags = dict_get_flags(entry);
            if (forth_state == STATE_COMPILE && !(flags & F_IMMEDIATE)) dict_comma((cell_t)(u64)entry);
            else forth_execute(entry);
            continue;
        }
        i64 num;
        if (try_parse_number(token, &num)) {
            if (forth_state == STATE_COMPILE) {
                dict_comma((cell_t)do_literal); dict_comma((cell_t)num);
            } else ds_push(num);
            continue;
        }
        vga_puts(token); vga_puts(" ? Unknown word\n");
        forth_state = STATE_INTERPRET;
        return;
    }
    if (forth_state == STATE_INTERPRET) vga_puts(" ok\n");
}

void forth_init(void) {
    ds_clear(); rs_clear(); dict_init(); register_primitives();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("\n  Vector Forth OS v0.1\n  Threaded-Code Compiler Ready\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/*=============================================================================
 * REPL (Read-Eval-Print Loop)
 *=============================================================================*/
void forth_run(void) {
    input_pos = 0;
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK); vga_puts("> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    while (1) {
        if (keyboard_available()) {
            char c = keyboard_getchar();
            
            if (c == '\n') {
                input_buffer[input_pos] = '\0'; 
                vga_putchar('\n');
                
                if (input_pos > 0) {
                    forth_eval(input_buffer);
                }
                
                input_pos = 0;
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                vga_puts(forth_state == STATE_COMPILE ? "| " : "> ");
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
            else if (c == '\b') { 
                if (input_pos > 0) {
                    input_pos--;
                    vga_putchar('\b'); vga_putchar(' '); vga_putchar('\b');
                } 
            }
            else if (c >= 32 && input_pos < INPUT_BUF_SIZE - 1) {
                vga_putchar(c); 
                input_buffer[input_pos++] = c;
            }
        } else {
            /* 1. Give background tasks a chance to run */
            yield();
            
            /* 2. Put CPU to sleep until the next hardware interrupt! 
             *    This stops QEMU from using 100% CPU and prevents input lag. */
            hlt();
        }
    }
}