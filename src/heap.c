#include "heap.h"
#include "string.h"
#include <stdint.h>

#define HEAP_START 0x00400000u
#define HEAP_SIZE  0x04000000u
#define ALIGN      16u

typedef struct block {
    size_t size;
    int free;
    struct block* next;
} block_t;

static block_t* head_block;
static size_t used_bytes;

void heap_init(void)
{
    head_block = (block_t*)(uintptr_t)HEAP_START;
    head_block->size = HEAP_SIZE - sizeof(block_t);
    head_block->free = 1;
    head_block->next = NULL;
    used_bytes = 0;
}

static size_t align_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1);
}

void* kmalloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = align_up(size);

    block_t* current = head_block;
    while (current) {
        if (current->free && current->size >= size) {
            if (current->size >= size + sizeof(block_t) + ALIGN) {
                block_t* split = (block_t*)((uint8_t*)current + sizeof(block_t) + size);
                split->size = current->size - size - sizeof(block_t);
                split->free = 1;
                split->next = current->next;
                current->size = size;
                current->next = split;
            }
            current->free = 0;
            used_bytes += current->size + sizeof(block_t);
            return (uint8_t*)current + sizeof(block_t);
        }
        current = current->next;
    }
    return NULL;
}

void kfree(void* ptr)
{
    if (!ptr)
        return;
    block_t* block = (block_t*)((uint8_t*)ptr - sizeof(block_t));
    block->free = 1;
    used_bytes -= block->size + sizeof(block_t);

    block_t* current = head_block;
    while (current) {
        if (current->free && current->next && current->next->free) {
            current->size += sizeof(block_t) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
}

size_t heap_used(void)
{
    return used_bytes;
}

size_t heap_total(void)
{
    return HEAP_SIZE;
}
