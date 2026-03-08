;=============================================================================
; Vector Forth Kernel - Stage 1 Bootloader (MBR - 512 bytes)
; Loads Stage 2 from disk, validates boot signature
;=============================================================================

[BITS 16]
[ORG 0x7C00]

; Load Stage 2 at linear address 0x8000
; We use Segment 0x0000 and Offset 0x8000 to match [ORG 0x8000] in Stage 2
STAGE2_LOAD_SEG     equ 0x0000
STAGE2_LOAD_OFF     equ 0x8000
STAGE2_SECTORS      equ 32          ; 16KB for stage 2
STAGE2_START_SECTOR equ 2           ; LBA sector 1 (0-indexed, sector after MBR)

;-----------------------------------------------------------------------------
; Entry point - BIOS loads us at 0x7C00
;-----------------------------------------------------------------------------
start:
    ; Setup segments
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00              ; Stack grows down from our load address
    sti

    ; Save boot drive number
    mov     [boot_drive], dl

    ; Print boot message
    mov     si, msg_booting
    call    print_string

    ; Reset disk system
    xor     ax, ax
    mov     dl, [boot_drive]
    int     0x13
    jc      disk_error

    ; Load Stage 2 using LBA via INT 13h extensions
    mov     si, dap
    mov     ah, 0x42
    mov     dl, [boot_drive]
    int     0x13
    jc      .try_chs

    ; Verify stage 2 signature
    mov     ax, STAGE2_LOAD_SEG
    mov     es, ax
    cmp     word [es:STAGE2_LOAD_OFF], 0x5646  ; 'VF' signature
    jne     sig_error

    ; Jump to stage 2 (0x0000:0x8002)
    mov     dl, [boot_drive]
    jmp     STAGE2_LOAD_SEG:STAGE2_LOAD_OFF + 2  ; Skip signature word

.try_chs:
    ; Fallback: CHS-based loading for older BIOS
    mov     ax, STAGE2_LOAD_SEG
    mov     es, ax
    mov     bx, STAGE2_LOAD_OFF

    mov     cx, STAGE2_SECTORS
    mov     ax, 1                   ; Start from sector 1 (second sector)

.read_loop:
    push    cx
    push    ax

    call    lba_to_chs
    mov     ah, 0x02
    mov     al, 1                   ; Read 1 sector
    mov     dl, [boot_drive]
    int     0x13
    jc      disk_error

    pop     ax
    pop     cx

    inc     ax
    add     bx, 512
    loop    .read_loop

    ; Verify stage 2 signature
    cmp     word [es:STAGE2_LOAD_OFF], 0x5646
    jne     sig_error

    mov     dl, [boot_drive]
    jmp     STAGE2_LOAD_SEG:STAGE2_LOAD_OFF + 2

;-----------------------------------------------------------------------------
; LBA to CHS conversion
; Input: AX = LBA
; Output: CH = cylinder, CL = sector, DH = head
;-----------------------------------------------------------------------------
lba_to_chs:
    push    bx
    xor     dx, dx
    mov     bx, 18                  ; Sectors per track (floppy default)
    div     bx
    inc     dl
    mov     cl, dl                  ; Sector (1-based)
    xor     dx, dx
    mov     bx, 2                   ; Number of heads
    div     bx
    mov     dh, dl                  ; Head
    mov     ch, al                  ; Cylinder
    pop     bx
    ret

;-----------------------------------------------------------------------------
; Print string (SI = pointer to null-terminated string)
;-----------------------------------------------------------------------------
print_string:
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

;-----------------------------------------------------------------------------
; Error handlers
;-----------------------------------------------------------------------------
disk_error:
    mov     si, msg_disk_err
    call    print_string
    jmp     halt

sig_error:
    mov     si, msg_sig_err
    call    print_string
    jmp     halt

halt:
    cli
    hlt
    jmp     halt

;-----------------------------------------------------------------------------
; Data
;-----------------------------------------------------------------------------
boot_drive:     db 0
msg_booting:    db 'VFK Boot', 13, 10, 0
msg_disk_err:   db 'Disk Err', 13, 10, 0
msg_sig_err:    db 'Sig Err', 13, 10, 0

; DAP (Disk Address Packet) for INT 13h extensions
align 4
dap:
    db  0x10                        ; Size of DAP
    db  0                           ; Reserved
    dw  STAGE2_SECTORS              ; Number of sectors
    dw  STAGE2_LOAD_OFF             ; Offset
    dw  STAGE2_LOAD_SEG             ; Segment
    dq  1                           ; Start LBA (sector 1)

;-----------------------------------------------------------------------------
; Boot signature
;-----------------------------------------------------------------------------
times 510 - ($ - $$) db 0
dw 0xAA55