#ifndef IDT_H
#define IDT_H

#include "../core/kernel.h"

/*--- IDT Entry (16 bytes in long mode) ---*/
typedef struct __attribute__((packed)) {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  flags;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} idt_entry_t;

/*--- IDTR ---*/
typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} idt_ptr_t;

/*--- Interrupt frame ---*/
typedef struct __attribute__((packed)) {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

/*--- IDT flags ---*/
#define IDT_PRESENT     0x80
#define IDT_DPL0        0x00
#define IDT_DPL3        0x60
#define IDT_INTERRUPT   0x0E
#define IDT_TRAP        0x0F

#define IDT_FLAGS_INTERRUPT (IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT)
#define IDT_FLAGS_TRAP      (IDT_PRESENT | IDT_DPL0 | IDT_TRAP)
#define IDT_FLAGS_USER      (IDT_PRESENT | IDT_DPL3 | IDT_INTERRUPT)

#define IDT_ENTRIES     256

/*--- PIC ---*/
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1
#define PIC_EOI         0x20

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01

#define IRQ_BASE        0x20

/*--- Functions ---*/
void idt_init(void);
void idt_set_gate(u8 vector, u64 handler, u16 selector, u8 flags, u8 ist);
void pic_init(void);
void pic_send_eoi(u8 irq);
void pic_set_mask(u8 irq);
void pic_clear_mask(u8 irq);

#endif