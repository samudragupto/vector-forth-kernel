;=============================================================================
; Context Switcher (x86_64 System V ABI)
;=============================================================================

[BITS 64]

global switch_context

; void switch_context(u64 *old_rsp_ptr, u64 new_rsp);
; RDI = pointer to old task's stack pointer variable
; RSI = new task's stack pointer

section .text
switch_context:
    ; 1. Save RFLAGS (Interrupt state)
    pushfq

    ; 2. Save callee-saved registers
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; 3. Save current stack pointer
    mov     [rdi], rsp

    ; 4. Load the new stack pointer
    mov     rsp, rsi

    ; 5. Restore registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp

    ; 6. Restore RFLAGS (This restores the interrupt state of the target task!)
    popfq

    ; 7. Return to new task
    ret