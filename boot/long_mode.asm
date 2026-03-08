;=============================================================================
; Stage 2 Bootloader - Transition to Long Mode and load kernel
;=============================================================================

[BITS 16]
[ORG 0x8000]

STAGE2_SECTORS      equ 32
dw  0x5646

stage2_entry:
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0xFFFC
    sti

    mov     [boot_drive_s2], dl

    mov     si, msg_stage2
    call    print16

    call    detect_memory
    call    enable_a20
    call    load_kernel

    cli
    call    install_gdt

    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax

    jmp     GDT_CODE_32:protected_mode_entry

detect_memory:
    mov     di, 0x6008              
    xor     ebx, ebx               
    xor     bp, bp                  
    mov     edx, 0x534D4150         
.e820_loop:
    mov     eax, 0xE820
    mov     ecx, 24                 
    int     0x15
    jc      .e820_done              
    cmp     eax, 0x534D4150         
    jne     .e820_done
    inc     bp
    add     di, 24
    test    ebx, ebx               
    jnz     .e820_loop
.e820_done:
    mov     [0x6000], bp            
    ret

enable_a20:
    mov     ax, 0x2401
    int     0x15
    jnc     .a20_done
    call    .a20_wait_input
    mov     al, 0xAD
    out     0x64, al
    call    .a20_wait_input
    mov     al, 0xD0
    out     0x64, al
    call    .a20_wait_output
    in      al, 0x60
    push    eax
    call    .a20_wait_input
    mov     al, 0xD1
    out     0x64, al
    call    .a20_wait_input
    pop     eax
    or      al, 2
    out     0x60, al
    call    .a20_wait_input
    mov     al, 0xAE
    out     0x64, al
    call    .a20_wait_input
.a20_done:
    ret
.a20_wait_input:
    in      al, 0x64
    test    al, 2
    jnz     .a20_wait_input
    ret
.a20_wait_output:
    in      al, 0x64
    test    al, 1
    jz      .a20_wait_output
    ret

KERNEL_LOAD_SEGMENT equ 0x0000
KERNEL_LOAD_TEMP    equ 0x7E00      
KERNEL_PHYS_ADDR    equ 0x100000    
KERNEL_SECTORS      equ 256         
KERNEL_START_LBA    equ 34          

load_kernel:
    mov     si, msg_loading
    call    print16
    mov     ax, 0x1000              
    mov     es, ax
    xor     bx, bx
    mov     cx, KERNEL_SECTORS
    mov     eax, KERNEL_START_LBA

.load_loop:
    push    cx
    push    eax
    mov     word [dap_s2 + 2], 1    
    mov     word [dap_s2 + 4], bx   
    mov     word [dap_s2 + 6], es   
    mov     dword [dap_s2 + 8], eax 
    mov     dword [dap_s2 + 12], 0  
    mov     si, dap_s2
    mov     ah, 0x42
    mov     dl, [boot_drive_s2]
    int     0x13
    jc      .load_error

    pop     eax
    pop     cx
    inc     eax                     
    add     bx, 512                 
    jnc     .no_overflow
    mov     dx, es
    add     dx, 0x1000
    mov     es, dx
    xor     bx, bx
.no_overflow:
    loop    .load_loop

    mov     si, msg_loaded
    call    print16
    ret

.load_error:
    mov     si, msg_load_err
    call    print16
    cli
    hlt

print16:
    pusha
.loop:
    lodsb
    test    al, al
    jz      .done
    mov     ah, 0x0E
    mov     bx, 0x0007
    int     0x10
    jmp     .loop
.done:
    popa
    ret

boot_drive_s2:  db 0
msg_stage2:     db 'Stage 2', 13, 10, 0
msg_loading:    db 'Loading kernel...', 13, 10, 0
msg_loaded:     db 'Kernel loaded', 13, 10, 0
msg_load_err:   db 'Load error!', 13, 10, 0

align 4
dap_s2:
    db  0x10                        
    db  0                           
    dw  1                           
    dw  0                           
    dw  0                           
    dq  0                           

;=============================================================================
; 32-BIT PROTECTED MODE
;=============================================================================
[BITS 32]
protected_mode_entry:
    mov     ax, GDT_DATA_32
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, 0x90000            

    ; Copy Kernel
    mov     esi, 0x10000            
    mov     edi, KERNEL_PHYS_ADDR   
    mov     ecx, (KERNEL_SECTORS * 512) / 4  
    rep     movsd

    ; Call paging setup (safely stored at bottom of file now)
    call    setup_paging

    mov     eax, cr4
    or      eax, (1 << 5)           
    mov     cr4, eax

    mov     ecx, 0xC0000080         
    rdmsr
    or      eax, (1 << 8)           
    wrmsr

    mov     eax, cr0
    or      eax, (1 << 31)          
    mov     cr0, eax

    jmp     GDT_CODE_64:long_mode_entry

;=============================================================================
; 64-BIT LONG MODE
;=============================================================================
[BITS 64]
long_mode_entry:
    mov     ax, GDT_DATA_64
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     rsp, 0x90000
    cld

    movzx   rdi, word [0x6000]      
    mov     rsi, 0x6008             

    ; Jump to entry.asm (_start -> kernel_main)
    mov     rax, 0x100000
    call    rax

    cli
.hang:
    hlt
    jmp     .hang

; ============================================================================
; INCLUDES PLACED SAFELY AT THE END
; ============================================================================
%include "gdt.asm"
%include "paging.asm"

times (STAGE2_SECTORS * 512) - ($ - $$) db 0