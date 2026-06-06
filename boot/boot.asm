[BITS 16]
[ORG 0x7C00]

KERNEL_LOAD_SEG equ 0x1000
KERNEL_LOAD_OFF equ 0x0000

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

    mov si, msg_load
    call print16

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
    mov word [dap_off], KERNEL_LOAD_OFF
    mov bx, [cur_lba]
    mov [dap_lba], bx
    mov word [dap_lba+2], 0
    mov dword [dap_lba+4], 0
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_err
    mov bx, [dap_count]
    sub cx, bx
    add [cur_lba], bx
    mov ax, bx
    mov dx, 0x20
    mul dx
    add [cur_seg], ax
    jmp load_loop

load_done:
    mov si, msg_ok
    call print16

    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode_entry

disk_err:
    mov si, msg_err
    call print16
.hang:
    hlt
    jmp .hang

print16:
    mov ah, 0x0E
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    ret

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

msg_load db "Orbit boot: loading kernel...", 13, 10, 0
msg_ok   db "ok", 13, 10, 0
msg_err  db "disk read error", 13, 10, 0

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
