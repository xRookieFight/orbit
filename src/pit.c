#include "pit.h"
#include "isr.h"
#include "io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE     1193182

static volatile uint32_t tick_count;
static uint32_t pit_hz;
static void (*idle_hook)(void);

static void pit_callback(regs_t* regs)
{
    (void)regs;
    tick_count++;
}

void pit_init(uint32_t frequency)
{
    pit_hz = frequency;
    uint32_t divisor = PIT_BASE / frequency;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_register(0, pit_callback);
}

uint32_t pit_ticks(void)
{
    return tick_count;
}

uint32_t pit_frequency(void)
{
    return pit_hz;
}

void pit_set_idle(void (*hook)(void))
{
    idle_hook = hook;
}

void pit_sleep(uint32_t ms)
{
    uint32_t target = tick_count + (ms * pit_hz) / 1000;
    while (tick_count < target) {
        if (idle_hook)
            idle_hook();
        __asm__ volatile("hlt");
    }
}
