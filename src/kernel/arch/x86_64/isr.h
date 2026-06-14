#pragma once
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} regs_t;

typedef void (*irq_handler_t)(regs_t* regs);

void isr_install(void);
void irq_register(int irq, irq_handler_t handler);
