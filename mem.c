/* OneOS-ARM Heap Memory Allocator */

#include "mem.h"

/* Free list head */
static mem_block_t *free_list = NULL;

/* Statistics */
static size_t total_allocated = 0;
static size_t total_free = 0;

/* Align size to 8-byte boundary */
#define ALIGN_SIZE(size) (((size) + 7) & ~7)

/* Get block from pointer */
#define BLOCK_FROM_PTR(ptr) ((mem_block_t *)((uint8_t *)(ptr) - sizeof(mem_block_t)))

/* Get pointer from block */
#define PTR_FROM_BLOCK(block) ((void *)((uint8_t *)(block) + sizeof(mem_block_t)))

void mem_init(void *start, size_t size)
{
    /* Initialize heap with one large free block */
    mem_block_t *initial_block = (mem_block_t *)start;
    initial_block->size = size - sizeof(mem_block_t);
    initial_block->next = NULL;
    initial_block->prev = NULL;
    initial_block->free = 1;

    free_list = initial_block;
    total_allocated = 0;
    total_free = size;
}

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;

    /* Align size */
    size = ALIGN_SIZE(size);

    /* Find first fit in free list */
    mem_block_t *current = free_list;
    while (current) {
        if (current->free && current->size >= size) {
            /* Found suitable block */
            if (current->size > size + sizeof(mem_block_t) + 8) {
                /* Split block */
                mem_block_t *new_block = (mem_block_t *)((uint8_t *)current + sizeof(mem_block_t) + size);
                new_block->size = current->size - size - sizeof(mem_block_t);
                new_block->next = current->next;
                new_block->prev = current;
                new_block->free = 1;

                if (current->next) {
                    current->next->prev = new_block;
                }

                current->next = new_block;
                current->size = size;
            }

            /* Mark as allocated */
            current->free = 0;

            /* Update statistics */
            total_allocated += current->size;
            total_free -= current->size + sizeof(mem_block_t);

            return PTR_FROM_BLOCK(current);
        }
        current = current->next;
    }

    /* No suitable block found */
    return NULL;
}

void kfree(void *ptr)
{
    if (!ptr) return;

    mem_block_t *block = BLOCK_FROM_PTR(ptr);

    /* Mark as free */
    block->free = 1;

    /* Update statistics */
    total_allocated -= block->size;
    total_free += block->size + sizeof(mem_block_t);

    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += sizeof(mem_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Coalesce with previous block if free */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(mem_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

size_t mem_get_used(void)
{
    return total_allocated;
}

size_t mem_get_free(void)
{
    return total_free;
}
