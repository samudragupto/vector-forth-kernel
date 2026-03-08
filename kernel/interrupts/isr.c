/*=============================================================================
 * ISR Handler - CPU Exception Dispatcher
 *=============================================================================*/

#include "idt.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../utils/string.h"
#include "../utils/stdlib.h"
#include "../core/kernel.h"

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved",
    "VMM Communication",
    "Security Exception",
    "Reserved"
};

/*--- ISR C handler ---*/
void isr_handler_c(interrupt_frame_t *frame) {
    u64 int_no = frame->int_no;

    if (int_no < 32) {
        /* CPU Exception */
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_clear();
        vga_puts("=== CPU EXCEPTION ===\n\n");

        if (int_no < 32) {
            vga_puts("Exception: ");
            vga_puts(exception_messages[int_no]);
            vga_putchar('\n');
        }

        char buf[32];

        vga_puts("INT: "); itoa((int)int_no, buf, 10); vga_puts(buf); vga_putchar('\n');
        vga_puts("ERR: "); vga_put_hex(frame->err_code); vga_putchar('\n');
        vga_puts("RIP: "); vga_put_hex(frame->rip); vga_putchar('\n');
        vga_puts("CS:  "); vga_put_hex(frame->cs); vga_putchar('\n');
        vga_puts("FLG: "); vga_put_hex(frame->rflags); vga_putchar('\n');
        vga_puts("RSP: "); vga_put_hex(frame->rsp); vga_putchar('\n');
        vga_puts("RAX: "); vga_put_hex(frame->rax); vga_putchar('\n');
        vga_puts("RBX: "); vga_put_hex(frame->rbx); vga_putchar('\n');
        vga_puts("RCX: "); vga_put_hex(frame->rcx); vga_putchar('\n');
        vga_puts("RDX: "); vga_put_hex(frame->rdx); vga_putchar('\n');

        if (int_no == 14) {
            /* Page fault - print CR2 */
            u64 cr2 = read_cr2();
            vga_puts("CR2: "); vga_put_hex(cr2); vga_putchar('\n');

            if (frame->err_code & 1) vga_puts("  Page-level protection violation\n");
            else vga_puts("  Page not present\n");
            if (frame->err_code & 2) vga_puts("  Write access\n");
            else vga_puts("  Read access\n");
            if (frame->err_code & 4) vga_puts("  User mode\n");
            else vga_puts("  Supervisor mode\n");
        }

        /* Serial debug output */
        serial_puts(SERIAL_COM1, "\n!!! EXCEPTION ");
        serial_puts(SERIAL_COM1, exception_messages[int_no]);
        serial_puts(SERIAL_COM1, " at RIP=");
        serial_put_hex(SERIAL_COM1, frame->rip);
        serial_puts(SERIAL_COM1, "\n");

        /* Halt on exception */
        vga_puts("\nSystem halted.\n");
        cli();
        while (1) hlt();
    }
}