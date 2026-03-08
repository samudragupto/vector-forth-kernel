;=============================================================================
; Paging Setup - Identity map first 4GB + Higher-half kernel mapping
; PML4 -> PDPT -> PD (2MB pages)
;=============================================================================

[BITS 32]

; Page table locations in physical memory
PML4_ADDR       equ 0x1000          ; Page Map Level 4
PDPT_ADDR       equ 0x2000          ; Page Directory Pointer Table
PD_ADDR         equ 0x3000          ; Page Directory (identity map)
PDPT_HIGH_ADDR  equ 0x4000          ; PDPT for higher half
PD_HIGH_ADDR    equ 0x5000          ; PD for higher half kernel

; Higher-half kernel base: 0xFFFF_8000_0000_0000
; PML4 index: 256 (bit 47 set -> sign extend to canonical)

PAGE_PRESENT    equ (1 << 0)
PAGE_WRITABLE   equ (1 << 1)
PAGE_HUGE       equ (1 << 7)        ; 2MB pages
PAGE_GLOBAL     equ (1 << 8)

PAGE_RW         equ (PAGE_PRESENT | PAGE_WRITABLE)
PAGE_RW_HUGE    equ (PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE)

;-----------------------------------------------------------------------------
; setup_paging - Create page tables for long mode
; Identity maps first 4GB and sets up higher-half mapping
;-----------------------------------------------------------------------------
setup_paging:
    ; Clear page table memory (0x1000 - 0x6000)
    mov     edi, PML4_ADDR
    xor     eax, eax
    mov     ecx, 0x5000 / 4        ; 5 pages worth of dwords
    rep     stosd

    ;--- PML4 Entry 0: Identity map ---
    mov     dword [PML4_ADDR], PDPT_ADDR | PAGE_RW
    mov     dword [PML4_ADDR + 4], 0

    ;--- PML4 Entry 256: Higher-half kernel (0xFFFF_8000_0000_0000) ---
    mov     dword [PML4_ADDR + 256 * 8], PDPT_HIGH_ADDR | PAGE_RW
    mov     dword [PML4_ADDR + 256 * 8 + 4], 0

    ;--- PML4 Entry 511: Recursive mapping ---
    mov     dword [PML4_ADDR + 511 * 8], PML4_ADDR | PAGE_RW
    mov     dword [PML4_ADDR + 511 * 8 + 4], 0

    ;--- PDPT Entry 0: Points to PD for first 1GB (identity) ---
    mov     dword [PDPT_ADDR], PD_ADDR | PAGE_RW
    mov     dword [PDPT_ADDR + 4], 0

    ;--- PDPT Entry 1-3: Direct 1GB pages for 1-4GB (if supported) ---
    ; Use 2MB pages instead for compatibility
    ; We'll map additional PDs if needed

    ;--- PD: Identity map first 1GB using 2MB pages ---
    mov     edi, PD_ADDR
    mov     eax, PAGE_RW_HUGE      ; Start at physical 0, 2MB pages
    mov     ecx, 512               ; 512 entries = 1GB
.identity_loop:
    mov     dword [edi], eax
    mov     dword [edi + 4], 0
    add     eax, 0x200000           ; Next 2MB
    add     edi, 8
    loop    .identity_loop

    ;--- PDPT Higher Half Entry 0: Points to PD_HIGH ---
    mov     dword [PDPT_HIGH_ADDR], PD_HIGH_ADDR | PAGE_RW
    mov     dword [PDPT_HIGH_ADDR + 4], 0

    ;--- PD Higher Half: Map kernel at physical 0x100000 (1MB) ---
    ; First entry: 0-2MB (includes kernel load area)
    mov     edi, PD_HIGH_ADDR
    mov     eax, PAGE_RW_HUGE      ; Physical 0x000000
    mov     ecx, 512               ; Map full 1GB higher half
.higher_loop:
    mov     dword [edi], eax
    mov     dword [edi + 4], 0
    add     eax, 0x200000
    add     edi, 8
    loop    .higher_loop

    ;--- Load PML4 into CR3 ---
    mov     eax, PML4_ADDR
    mov     cr3, eax

    ret