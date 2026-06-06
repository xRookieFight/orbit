[BITS 32]

section .text.boot
global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    mov esp, stack_top
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    xor ebp, ebp
    call kmain
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:
