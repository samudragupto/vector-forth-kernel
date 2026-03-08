;=============================================================================
; ISR and IRQ Assembly Stubs for x86-64 Long Mode
;=============================================================================

[BITS 64]

;--- External C handlers ---
extern isr_handler_c
extern irq_handler_c

;--- Export ISR stubs ---
global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

;--- Export IRQ stubs ---
global irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
global irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15

;--- Export IDT load ---
global idt_load

;=============================================================================
; IDT Load
;=============================================================================
idt_load:
    lidt    [rdi]
    ret

;=============================================================================
; Common ISR stub - saves all registers, calls C handler
;=============================================================================
isr_common_stub:
    ; Save all general-purpose registers
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Pass pointer to interrupt frame as argument
    mov     rdi, rsp

    ; Align stack to 16 bytes (ABI requirement)
    mov     rbp, rsp
    and     rsp, ~0xF
    sub     rsp, 8      ; Ensure 16-byte alignment after call

    call    isr_handler_c

    ; Restore stack
    mov     rsp, rbp

    ; Restore all registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Remove int_no and err_code from stack
    add     rsp, 16

    iretq

;=============================================================================
; Common IRQ stub
;=============================================================================
irq_common_stub:
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rdi, rsp

    mov     rbp, rsp
    and     rsp, ~0xF
    sub     rsp, 8

    call    irq_handler_c

    mov     rsp, rbp

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    add     rsp, 16
    iretq

;=============================================================================
; ISR stubs (exceptions 0-31)
; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
;=============================================================================

; Macro for ISR without error code (push dummy 0)
%macro ISR_NOERR 1
isr%1:
    push    qword 0         ; Dummy error code
    push    qword %1        ; Interrupt number
    jmp     isr_common_stub
%endmacro

; Macro for ISR with error code (CPU pushes it)
%macro ISR_ERR 1
isr%1:
    ; Error code already pushed by CPU
    push    qword %1        ; Interrupt number
    jmp     isr_common_stub
%endmacro

; Exception stubs
ISR_NOERR 0     ; Divide by zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound range exceeded
ISR_NOERR 6     ; Invalid opcode
ISR_NOERR 7     ; Device not available
ISR_ERR   8     ; Double fault
ISR_NOERR 9     ; Coprocessor segment overrun
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment not present
ISR_ERR   12    ; Stack-segment fault
ISR_ERR   13    ; General protection fault
ISR_ERR   14    ; Page fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 floating-point
ISR_ERR   17    ; Alignment check
ISR_NOERR 18    ; Machine check
ISR_NOERR 19    ; SIMD floating-point
ISR_NOERR 20    ; Virtualization
ISR_ERR   21    ; Control protection
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29    ; VMM communication
ISR_ERR   30    ; Security
ISR_NOERR 31

;=============================================================================
; IRQ stubs (hardware interrupts mapped to 32-47)
;=============================================================================

%macro IRQ 2
irq%1:
    push    qword 0         ; Dummy error code
    push    qword %2        ; Interrupt number (32+)
    jmp     irq_common_stub
%endmacro

IRQ 0,  32      ; PIT Timer
IRQ 1,  33      ; Keyboard
IRQ 2,  34      ; Cascade
IRQ 3,  35      ; COM2
IRQ 4,  36      ; COM1
IRQ 5,  37      ; LPT2
IRQ 6,  38      ; Floppy
IRQ 7,  39      ; LPT1 / Spurious
IRQ 8,  40      ; RTC
IRQ 9,  41      ; Free
IRQ 10, 42      ; Free
IRQ 11, 43      ; Free
IRQ 12, 44      ; PS/2 Mouse
IRQ 13, 45      ; FPU
IRQ 14, 46      ; Primary ATA
IRQ 15, 47      ; Secondary ATA