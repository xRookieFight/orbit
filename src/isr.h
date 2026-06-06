#pragma once
#include <stdint.h>

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} regs_t;

typedef void (*irq_handler_t)(regs_t* regs);

void isr_install(void);
void irq_register(int irq, irq_handler_t handler);
