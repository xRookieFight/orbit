#include "fb.h"
#include "heap.h"
#include "string.h"

#define VBE_INFO ((volatile uint8_t*)0x8000)

static uint8_t* front;
static uint32_t* back;
static int width;
static int height;
static int pitch;
static int ready;

void fb_init(void)
{
    pitch = *(volatile uint16_t*)(VBE_INFO + 0x10);
    width = *(volatile uint16_t*)(VBE_INFO + 0x12);
    height = *(volatile uint16_t*)(VBE_INFO + 0x14);
    front = (uint8_t*)(uintptr_t)(*(volatile uint32_t*)(VBE_INFO + 0x28));

    if (!front || width < 320 || height < 200)
        return;

    back = (uint32_t*)kmalloc((size_t)width * (size_t)height * 4);
    if (!back)
        return;

    memset(back, 0, (size_t)width * (size_t)height * 4);
    ready = 1;
}

int fb_ready(void)
{
    return ready;
}

int fb_width(void)
{
    return width;
}

int fb_height(void)
{
    return height;
}

uint32_t* fb_backbuffer(void)
{
    return back;
}

void fb_flip(void)
{
    if (!ready)
        return;
    for (int y = 0; y < height; y++)
        memcpy(front + (size_t)y * (size_t)pitch, back + (size_t)y * (size_t)width, (size_t)width * 4);
}
