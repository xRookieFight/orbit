[BITS 32]

section .text.boot
global _start
extern kmain
extern __bss_start
extern __bss_end

PML4 equ 0x1000
PDPT equ 0x2000
PD0  equ 0x3000

_start:
    cli
    mov esp, 0x90000

    mov edi, PML4
    xor eax, eax
    mov ecx, (0x7000 - 0x1000) / 4
    rep stosd

    mov dword [PML4], PDPT + 3
    mov dword [PDPT], PD0 + 3
    mov dword [PDPT + 8], PD0 + 0x1000 + 3
    mov dword [PDPT + 16], PD0 + 0x2000 + 3
    mov dword [PDPT + 24], PD0 + 0x3000 + 3

    mov edi, PD0
    mov eax, 0x83
    mov ecx, 2048
.fill_pd:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    mov eax, PML4
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    lgdt [gdt64.ptr]
    jmp 0x08:long_start

[BITS 64]
long_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb

    mov rsp, stack_top
    xor ebp, ebp
    call kmain
.hang:
    cli
    hlt
    jmp .hang

align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
.ptr:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:
