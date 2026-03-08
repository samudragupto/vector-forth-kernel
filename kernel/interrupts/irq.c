/*=============================================================================
 * IRQ Handler - Hardware Interrupt Dispatcher
 *=============================================================================*/

#include "idt.h"
#include "../core/kernel.h"
#include "../drivers/serial.h"

extern void timer_handler_c(void);
extern void keyboard_handler_c(void);

void irq_handler_c(interrupt_frame_t *frame) {
    u64 irq = frame->int_no - IRQ_BASE;

    switch (irq) {
        case 0:     
            timer_handler_c(); 
            break;
        case 1:     
            keyboard_handler_c(); 
            break;
        case 7:     
            outb(PIC1_COMMAND, 0x0B);
            if (!(inb(PIC1_COMMAND) & 0x80)) return;
            break;
        case 15:    
            outb(PIC2_COMMAND, 0x0B);
            if (!(inb(PIC2_COMMAND) & 0x80)) {
                outb(PIC1_COMMAND, PIC_EOI);
                return;
            }
            break;
    }

    /* Send End of Interrupt so hardware can fire again */
    pic_send_eoi((u8)irq);
}