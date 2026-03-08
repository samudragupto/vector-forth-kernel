;=============================================================================
; Kernel 64-bit Entry Stub
;=============================================================================

[BITS 64]

global _start
extern kernel_main

section .text.entry
_start:
    ; 1. Clear interrupts
    cli

    ; 2. Print 'E' 'N' in bright green to prove we made it to 64-bit Assembly
    mov rax, 0xB8004
    mov word [rax], 0x2F45  ; 'E'
    mov word [rax+2], 0x2F4E ; 'N'

    ; 3. Clear RFLAGS
    push    0
    popf

    ; 4. Call C code
    call    kernel_main

.hang:
    cli
    hlt
    jmp     .hang