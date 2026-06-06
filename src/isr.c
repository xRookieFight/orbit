#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "console.h"
#include "vga.h"
#include "io.h"

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static void* isr_stubs[32] = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void* irq_stubs[16] = {
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

static irq_handler_t irq_routines[16];

static const char* exception_messages[32] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point", "Alignment check", "Machine check", "SIMD floating-point",
    "Virtualization", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security", "Reserved"
};

void isr_install(void)
{
    pic_remap();

    for (int i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, (uint32_t)isr_stubs[i], 0x08, 0x8E);
    for (int i = 0; i < 16; i++)
        idt_set_gate((uint8_t)(32 + i), (uint32_t)irq_stubs[i], 0x08, 0x8E);
}

void irq_register(int irq, irq_handler_t handler)
{
    if (irq >= 0 && irq < 16)
        irq_routines[irq] = handler;
}

void isr_handler(regs_t* regs)
{
    console_set_color(VGA_LIGHT_RED, VGA_BLACK);
    console_printf("\n*** Orbit kernel panic ***\n");
    if (regs->int_no < 32)
        console_printf("Exception: %s\n", exception_messages[regs->int_no]);
    console_printf("int=%d err=%d eip=%p\n", regs->int_no, regs->err_code, (void*)regs->eip);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (;;)
        __asm__ volatile("cli; hlt");
}

void irq_handler(regs_t* regs)
{
    int irq = (int)regs->int_no - 32;
    if (irq >= 0 && irq < 16 && irq_routines[irq])
        irq_routines[irq](regs);
    pic_send_eoi(irq);
}
