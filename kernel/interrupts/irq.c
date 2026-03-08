/*=============================================================================
 * IRQ Handler - Hardware Interrupt Dispatcher
 *=============================================================================*/

#include "idt.h"
#include "../core/kernel.h"
#include "../drivers/serial.h"
#include "../utils/stdlib.h"  /* Added for itoa */

/*--- External device handlers ---*/
extern void timer_handler_c(void);
extern void keyboard_handler_c(void);

/*--- IRQ C handler ---*/
void irq_handler_c(interrupt_frame_t *frame) {
    u64 irq = frame->int_no - IRQ_BASE;

    switch (irq) {
        case 0:     /* Timer (PIT) */
            timer_handler_c();
            break;

        case 1:     /* Keyboard */
            keyboard_handler_c();
            break;

        case 7:     /* Spurious IRQ (PIC1) */
        {
            /* Check if it's a real IRQ by reading ISR */
            outb(PIC1_COMMAND, 0x0B);  /* Read ISR */
            u8 isr = inb(PIC1_COMMAND);
            if (!(isr & 0x80)) {
                /* Spurious - don't send EOI */
                return;
            }
            break;
        }

        case 15:    /* Spurious IRQ (PIC2) */
        {
            outb(PIC2_COMMAND, 0x0B);
            u8 isr = inb(PIC2_COMMAND);
            if (!(isr & 0x80)) {
                /* Spurious from slave - send EOI to master only */
                outb(PIC1_COMMAND, PIC_EOI);
                return;
            }
            break;
        }

        default:
            /* Unhandled IRQ - log to serial */
            serial_puts(SERIAL_COM1, "Unhandled IRQ: ");
            {
                char buf[4];
                itoa((int)irq, buf, 10);
                serial_puts(SERIAL_COM1, buf);
            }
            serial_puts(SERIAL_COM1, "\n");
            break;
    }

    /* Send End of Interrupt */
    pic_send_eoi((u8)irq);
}