/* OneOS-ARM Memory Management */

#ifndef MEM_H
#define MEM_H

/* Define basic types for bare-metal */
typedef unsigned char uint8_t;
typedef unsigned int size_t;
#define NULL ((void*)0)

/* Memory block header for heap allocator */
typedef struct mem_block {
    size_t size;                    /* Size of this block (excluding header) */
    struct mem_block *next;         /* Next block in free list */
    struct mem_block *prev;         /* Previous block in free list */
    uint8_t free;                   /* 1 if free, 0 if allocated */
} mem_block_t;

/* Initialize heap allocator */
void mem_init(void *heap_start, size_t heap_size);

/* Allocate memory */
void *kmalloc(size_t size);

/* Free memory */
void kfree(void *ptr);

/* Get heap statistics */
size_t mem_get_used(void);
size_t mem_get_free(void);

#endif
