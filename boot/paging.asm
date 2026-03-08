;=============================================================================
; Paging Setup - Identity map first 2MB + Higher-half kernel mapping
; PML4 -> PDPT -> PD (2MB pages)
;
; Virtual Memory Layout:
;   0x0000_0000_0000_0000 - 0x0000_0000_001F_FFFF : Identity map (first 2MB)
;   0xFFFF_8000_0000_0000 - 0xFFFF_8000_001F_FFFF : Higher-half kernel (2MB)
;   0xFFFF_8000_0020_0000 - 0xFFFF_8000_003F_FFFF : Kernel heap (2MB)
;   (Identity map kept temporarily so bootloader code keeps running
;    until we jump to the higher-half addresses)
;=============================================================================

[BITS 32]

; Page table physical addresses (below kernel at 1MB)
; Each table is 4KB (one page)
PML4_ADDR           equ 0x70000     ; Page Map Level 4
PDPT_IDENT_ADDR     equ 0x71000     ; PDPT for identity map (entry 0)
PD_IDENT_ADDR       equ 0x72000     ; PD for identity map
PDPT_HIGHER_ADDR    equ 0x73000     ; PDPT for higher half (entry 256)
PD_HIGHER_ADDR      equ 0x74000     ; PD for higher half

PAGE_PRESENT        equ (1 << 0)
PAGE_WRITABLE       equ (1 << 1)
PAGE_HUGE           equ (1 << 7)

PAGE_RW             equ (PAGE_PRESENT | PAGE_WRITABLE)
PAGE_RW_HUGE        equ (PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE)

;-----------------------------------------------------------------------------
; setup_paging - Build 4-level page tables for long mode
;-----------------------------------------------------------------------------
setup_paging:
    ; Clear all page table memory
    mov     edi, PML4_ADDR
    xor     eax, eax
    mov     ecx, (5 * 4096) / 4        ; 5 pages, clear as dwords
    rep     stosd

    ; =========================================================
    ; PML4[0] -> PDPT_IDENT (identity map: VA 0x0 -> PA 0x0)
    ; =========================================================
    mov     eax, PDPT_IDENT_ADDR | PAGE_RW
    mov     dword [PML4_ADDR + 0*8], eax
    mov     dword [PML4_ADDR + 0*8 + 4], 0

    ; =========================================================
    ; PML4[256] -> PDPT_HIGHER (higher half: VA 0xFFFF800000000000)
    ; PML4 index for 0xFFFF800000000000:
    ;   Bits 47:39 of the virtual address = 256
    ; =========================================================
    mov     eax, PDPT_HIGHER_ADDR | PAGE_RW
    mov     dword [PML4_ADDR + 256*8], eax
    mov     dword [PML4_ADDR + 256*8 + 4], 0

    ; =========================================================
    ; PDPT_IDENT[0] -> PD_IDENT
    ; =========================================================
    mov     eax, PD_IDENT_ADDR | PAGE_RW
    mov     dword [PDPT_IDENT_ADDR + 0*8], eax
    mov     dword [PDPT_IDENT_ADDR + 0*8 + 4], 0

    ; =========================================================
    ; PD_IDENT: Identity map first 4MB using 2x 2MB pages
    ;   Entry 0: VA 0x000000 -> PA 0x000000 (2MB)
    ;   Entry 1: VA 0x200000 -> PA 0x200000 (2MB)
    ; =========================================================
    ; Entry 0: 0x000000
    mov     eax, 0x000000 | PAGE_RW_HUGE
    mov     dword [PD_IDENT_ADDR + 0*8], eax
    mov     dword [PD_IDENT_ADDR + 0*8 + 4], 0

    ; Entry 1: 0x200000
    mov     eax, 0x200000 | PAGE_RW_HUGE
    mov     dword [PD_IDENT_ADDR + 1*8], eax
    mov     dword [PD_IDENT_ADDR + 1*8 + 4], 0

    ; =========================================================
    ; PDPT_HIGHER[0] -> PD_HIGHER
    ; =========================================================
    mov     eax, PD_HIGHER_ADDR | PAGE_RW
    mov     dword [PDPT_HIGHER_ADDR + 0*8], eax
    mov     dword [PDPT_HIGHER_ADDR + 0*8 + 4], 0

    ; =========================================================
    ; PD_HIGHER: Map higher-half to physical memory
    ;   VA 0xFFFF800000000000 -> PA 0x000000 (2MB) [VGA, low mem]
    ;   VA 0xFFFF800000200000 -> PA 0x200000 (2MB) [kernel heap, PMM bitmap]
    ;   VA 0xFFFF800000400000 -> PA 0x400000 (2MB)
    ;   ... map 16 entries = 32MB of higher-half space
    ; =========================================================
    mov     edi, PD_HIGHER_ADDR
    mov     eax, PAGE_RW_HUGE          ; Start at PA 0x000000
    mov     ecx, 16                    ; 16 x 2MB = 32MB

.higher_loop:
    mov     dword [edi], eax
    mov     dword [edi + 4], 0
    add     eax, 0x200000              ; Next 2MB physical
    add     edi, 8                     ; Next PD entry
    loop    .higher_loop

    ; =========================================================
    ; Load PML4 into CR3
    ; =========================================================
    mov     eax, PML4_ADDR
    mov     cr3, eax

    ret
