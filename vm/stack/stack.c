#include "stack.h"
#include "../../kernel/drivers/vga.h"
#include "../../kernel/drivers/serial.h"
#include "../../kernel/utils/stdlib.h"

/*=============================================================================
 * Data Stack
 *=============================================================================*/
static i64 ds[DATA_STACK_SIZE];
static int dsp = 0;

void ds_push(i64 v) {
    if (dsp >= DATA_STACK_SIZE) {
        vga_puts(" Data Stack Overflow!\n");
        return;
    }
    ds[dsp++] = v;
}

i64 ds_pop(void) {
    if (dsp <= 0) {
        vga_puts(" Data Stack Underflow!\n");
        return 0;
    }
    return ds[--dsp];
}

i64 ds_peek(void) {
    if (dsp <= 0) {
        vga_puts(" Stack Empty!\n");
        return 0;
    }
    return ds[dsp - 1];
}

i64 ds_pick(int n) {
    if (n < 0 || n >= dsp) {
        vga_puts(" Invalid PICK!\n");
        return 0;
    }
    return ds[dsp - 1 - n];
}

i64 ds_depth(void) {
    return (i64)dsp;
}

void ds_clear(void) {
    dsp = 0;
}

void ds_print(void) {
    char buf[24];
    vga_puts("<");
    itoa(dsp, buf, 10);
    vga_puts(buf);
    vga_puts("> ");
    for (int i = 0; i < dsp; i++) {
        ltoa((long)ds[i], buf, 10);
        vga_puts(buf);
        vga_putchar(' ');
    }
}

/*=============================================================================
 * Return Stack
 *=============================================================================*/
static u64 rs[RETURN_STACK_SIZE];
static int rsp_idx = 0;

void rs_push(u64 v) {
    if (rsp_idx >= RETURN_STACK_SIZE) {
        kernel_panic("Return Stack Overflow!");
    }
    rs[rsp_idx++] = v;
}

u64 rs_pop(void) {
    if (rsp_idx <= 0) {
        kernel_panic("Return Stack Underflow!");
    }
    return rs[--rsp_idx];
}

u64 rs_peek(void) {
    if (rsp_idx <= 0) return 0;
    return rs[rsp_idx - 1];
}

u64 rs_depth(void) {
    return (u64)rsp_idx;
}

void rs_clear(void) {
    rsp_idx = 0;
}