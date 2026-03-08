/*=============================================================================
 * IDT setup and PIC initialization
 *=============================================================================*/

#include "idt.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../utils/string.h"

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idtr;

/*--- External ISR/IRQ assembly stubs ---*/
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

extern void idt_load(u64 idtr_addr);

/*--- Set an IDT gate ---*/
void idt_set_gate(u8 vector, u64 handler, u16 selector, u8 flags, u8 ist) {
    idt[vector].offset_low  = (u16)(handler & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].ist         = ist & 0x7;
    idt[vector].flags       = flags;
    idt[vector].offset_mid  = (u16)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (u32)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

/*--- Initialize PIC (8259) ---*/
void pic_init(void) {
    /* Removed unused read of current masks - we overwrite them anyway */

    /* Start initialization sequence */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* Set vector offsets */
    outb(PIC1_DATA, IRQ_BASE);          /* IRQ 0-7  -> INT 0x20-0x27 */
    io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);      /* IRQ 8-15 -> INT 0x28-0x2F */
    io_wait();

    /* Tell PICs about each other */
    outb(PIC1_DATA, 0x04);              /* IRQ2 has slave */
    io_wait();
    outb(PIC2_DATA, 0x02);              /* Slave identity */
    io_wait();

    /* 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore masks (mask all, will unmask as needed) */
    outb(PIC1_DATA, 0xFC);  /* Unmask IRQ0 (timer) and IRQ1 (keyboard) */
    outb(PIC2_DATA, 0xFF);  /* Mask all slave IRQs */
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(u8 irq) {
    u16 port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(u8 irq) {
    u16 port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) & ~(1 << irq));
}

/*--- Initialize IDT ---*/
void idt_init(void) {
    /* Clear IDT */
    memset(idt, 0, sizeof(idt));

    /* Initialize PIC */
    pic_init();

    /* Install ISR handlers (exceptions 0-31) */
    idt_set_gate(0,  (u64)isr0,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(1,  (u64)isr1,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(2,  (u64)isr2,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(3,  (u64)isr3,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(4,  (u64)isr4,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(5,  (u64)isr5,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(6,  (u64)isr6,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(7,  (u64)isr7,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(8,  (u64)isr8,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(9,  (u64)isr9,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(10, (u64)isr10, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(11, (u64)isr11, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(12, (u64)isr12, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(13, (u64)isr13, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(14, (u64)isr14, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(15, (u64)isr15, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(16, (u64)isr16, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(17, (u64)isr17, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(18, (u64)isr18, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(19, (u64)isr19, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(20, (u64)isr20, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(21, (u64)isr21, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(22, (u64)isr22, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(23, (u64)isr23, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(24, (u64)isr24, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(25, (u64)isr25, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(26, (u64)isr26, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(27, (u64)isr27, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(28, (u64)isr28, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(29, (u64)isr29, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(30, (u64)isr30, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(31, (u64)isr31, 0x08, IDT_FLAGS_INTERRUPT, 0);

    /* Install IRQ handlers (32-47) */
    idt_set_gate(32, (u64)irq0,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(33, (u64)irq1,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(34, (u64)irq2,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(35, (u64)irq3,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(36, (u64)irq4,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(37, (u64)irq5,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(38, (u64)irq6,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(39, (u64)irq7,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(40, (u64)irq8,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(41, (u64)irq9,  0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(42, (u64)irq10, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(43, (u64)irq11, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(44, (u64)irq12, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(45, (u64)irq13, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(46, (u64)irq14, 0x08, IDT_FLAGS_INTERRUPT, 0);
    idt_set_gate(47, (u64)irq15, 0x08, IDT_FLAGS_INTERRUPT, 0);

    /* Load IDT */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (u64)&idt;
    idt_load((u64)&idtr);
}