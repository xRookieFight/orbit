[BITS 16]
[ORG 0x7C00]

KERNEL_LOAD_SEG equ 0x1000

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 96
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    sti

    mov word [cur_lba], 1
    mov word [cur_seg], KERNEL_LOAD_SEG
    mov cx, KERNEL_SECTORS

load_loop:
    cmp cx, 0
    je load_done
    mov ax, cx
    cmp ax, 32
    jbe .have
    mov ax, 32
.have:
    mov [dap_count], ax
    mov bx, [cur_seg]
    mov [dap_seg], bx
    mov word [dap_off], 0
    mov bx, [cur_lba]
    mov [dap_lba], bx
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc fail
    mov bx, [dap_count]
    sub cx, bx
    add [cur_lba], bx
    mov ax, bx
    mov dx, 0x20
    mul dx
    add [cur_seg], ax
    jmp load_loop

load_done:
    mov word [want_w], 1920
    mov word [want_h], 1080
    call vbe_find
    jnc video_ok
    mov word [want_w], 1280
    mov word [want_h], 720
    call vbe_find
    jnc video_ok
    mov word [want_w], 1024
    mov word [want_h], 768
    call vbe_find
    jnc video_ok
    mov word [want_w], 800
    mov word [want_h], 600
    call vbe_find
    jnc video_ok
    jmp fail

video_ok:
    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode_entry

vbe_find:
    mov ax, 0x4F00
    xor di, di
    mov es, di
    mov di, 0x500
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov si, [0x50E]
    mov ax, [0x510]
    mov fs, ax
.next:
    mov ax, [fs:si]
    add si, 2
    cmp ax, 0xFFFF
    je .fail
    mov [cur_mode], ax
    mov cx, ax
    mov ax, 0x4F01
    xor di, di
    mov es, di
    mov di, 0x8000
    int 0x10
    cmp ax, 0x004F
    jne .next
    cmp byte [0x8019], 32
    jne .next
    mov ax, [0x8012]
    cmp ax, [want_w]
    jne .next
    mov ax, [0x8014]
    cmp ax, [want_h]
    jne .next
    test byte [0x8000], 0x80
    jz .next
    mov bx, [cur_mode]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .next
    clc
    ret
.fail:
    stc
    ret

fail:
    mov si, msg_err
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print
.hang:
    hlt
    jmp .hang

[BITS 32]
pmode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov eax, 0x10000
    jmp eax

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive db 0
cur_lba    dw 0
cur_seg    dw 0
cur_mode   dw 0
want_w     dw 0
want_h     dw 0

msg_err db "boot error", 0

align 4
dap:
dap_size  db 0x10
dap_resv  db 0
dap_count dw 0
dap_off   dw 0
dap_seg   dw 0
dap_lba   dq 0

times 510-($-$$) db 0
dw 0xAA55
