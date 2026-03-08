;=============================================================================
; Kernel 64-bit Entry Stub (Higher Half)
;=============================================================================

[BITS 64]

global _start
extern kernel_main

; Higher-half base
KERNEL_VIRT_BASE    equ 0xFFFF800000000000

section .text.entry
_start:
    cli

    ; Clear RFLAGS
    push    0
    popf

    ; RDI and RSI already set by bootloader trampoline
    ; Call the C kernel
    call    kernel_main

.hang:
    cli
    hlt
    jmp     .hang