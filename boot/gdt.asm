;=============================================================================
; GDT - Global Descriptor Table for Long Mode (64-bit)
;=============================================================================

[BITS 16]

;-----------------------------------------------------------------------------
; Install GDT and load GDTR
;-----------------------------------------------------------------------------
install_gdt:
    cli
    lgdt    [gdt_descriptor]
    ret

;-----------------------------------------------------------------------------
; GDT Table
; For long mode, most fields are ignored except:
;   - Code segment: L bit set, D bit clear
;   - Data segment: writable
;-----------------------------------------------------------------------------
align 16
gdt_start:

gdt_null:                           ; 0x00: Null descriptor (required)
    dq 0x0000000000000000

gdt_code_64:                        ; 0x08: 64-bit Code segment
    dw 0xFFFF                       ; Limit (ignored in long mode)
    dw 0x0000                       ; Base low
    db 0x00                         ; Base mid
    db 10011010b                    ; Access: Present, Ring 0, Code, Execute/Read
    db 10101111b                    ; Flags: Granularity, Long mode, Limit high
    db 0x00                         ; Base high

gdt_data_64:                        ; 0x10: 64-bit Data segment
    dw 0xFFFF                       ; Limit
    dw 0x0000                       ; Base low
    db 0x00                         ; Base mid
    db 10010010b                    ; Access: Present, Ring 0, Data, Read/Write
    db 11001111b                    ; Flags: Granularity, 32-bit size, Limit high
    db 0x00                         ; Base high

gdt_code_16:                        ; 0x18: 16-bit Code (for BIOS calls if needed)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 00000000b                    ; 16-bit
    db 0x00

gdt_data_16:                        ; 0x20: 16-bit Data
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00

gdt_code_32:                        ; 0x28: 32-bit Code (transitional)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b                    ; 32-bit, 4K granularity
    db 0x00

gdt_data_32:                        ; 0x30: 32-bit Data (transitional)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_tss:                            ; 0x38: TSS descriptor (16 bytes in long mode)
    dw 0x0067                       ; Limit (103 bytes for TSS)
    dw 0x0000                       ; Base low (filled at runtime)
    db 0x00                         ; Base mid
    db 10001001b                    ; Access: Present, TSS (Available)
    db 00000000b                    ; Flags
    db 0x00                         ; Base high
    dd 0x00000000                   ; Base upper 32 bits
    dd 0x00000000                   ; Reserved

gdt_end:

;-----------------------------------------------------------------------------
; GDT Descriptor (GDTR value)
;-----------------------------------------------------------------------------
gdt_descriptor:
    dw gdt_end - gdt_start - 1     ; Size
    dd gdt_start                    ; Offset (will be updated for long mode)

;-----------------------------------------------------------------------------
; Segment selectors
;-----------------------------------------------------------------------------
GDT_CODE_64     equ gdt_code_64 - gdt_start    ; 0x08
GDT_DATA_64     equ gdt_data_64 - gdt_start    ; 0x10
GDT_CODE_16     equ gdt_code_16 - gdt_start    ; 0x18
GDT_DATA_16     equ gdt_data_16 - gdt_start    ; 0x20
GDT_CODE_32     equ gdt_code_32 - gdt_start    ; 0x28
GDT_DATA_32     equ gdt_data_32 - gdt_start    ; 0x30
GDT_TSS         equ gdt_tss - gdt_start        ; 0x38