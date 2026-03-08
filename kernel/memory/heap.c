/*=============================================================================
 * Kernel Heap Allocator implementation
 * Simple first-fit free-list with block splitting and coalescing
 *=============================================================================*/

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../utils/string.h"
#include "../drivers/serial.h"

/*--- Heap block header ---*/
typedef struct heap_block {
    u64                 size;       /* Size of data area (excludes header) */
    struct heap_block  *next;       /* Next block in free list */
    u64                 magic;      /* Magic number for corruption detection */
    u8                  free;       /* 1 = free, 0 = allocated */
    u8                  padding[7]; /* Align to 16 bytes */
} __attribute__((packed)) heap_block_t;

#define HEAP_MAGIC      0xDEADBEEFCAFEBABEULL
#define HEAP_HEADER_SIZE sizeof(heap_block_t)
#define HEAP_MIN_BLOCK  64

/*--- Heap configuration ---*/
/* Heap starts at physical 0x300000 (3MB) with identity mapping */
#define HEAP_START      0x300000ULL
#define HEAP_INITIAL_SIZE (1024 * 1024)  /* 1MB initial heap */
#define HEAP_MAX_SIZE   (16 * 1024 * 1024) /* 16MB max heap */

static heap_block_t *heap_head = NULL;
static u64           heap_end  = 0;
static size_t        heap_total = 0;
static size_t        heap_used  = 0;

/*--- Initialize heap ---*/
void heap_init(void) {
    /* Mark initial heap pages as used in PMM */
    pmm_mark_region_used(HEAP_START, HEAP_INITIAL_SIZE);

    /* Setup initial free block */
    heap_head = (heap_block_t *)HEAP_START;
    heap_head->size  = HEAP_INITIAL_SIZE - HEAP_HEADER_SIZE;
    heap_head->next  = NULL;
    heap_head->magic = HEAP_MAGIC;
    heap_head->free  = 1;

    heap_end   = HEAP_START + HEAP_INITIAL_SIZE;
    heap_total = HEAP_INITIAL_SIZE;
    heap_used  = 0;

    serial_puts(SERIAL_COM1, "Heap: Initialized at ");
    serial_put_hex(SERIAL_COM1, HEAP_START);
    serial_puts(SERIAL_COM1, ", size=1MB\n");
}

/*--- Split a block if it's large enough ---*/
static void heap_split(heap_block_t *block, size_t size) {
    if (block->size <= size + HEAP_HEADER_SIZE + HEAP_MIN_BLOCK) {
        return; /* Not worth splitting */
    }

    heap_block_t *new_block = (heap_block_t *)((u8 *)block + HEAP_HEADER_SIZE + size);
    new_block->size  = block->size - size - HEAP_HEADER_SIZE;
    new_block->next  = block->next;
    new_block->magic = HEAP_MAGIC;
    new_block->free  = 1;

    block->size = size;
    block->next = new_block;
}

/*--- Coalesce adjacent free blocks ---*/
static void heap_coalesce(void) {
    heap_block_t *block = heap_head;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += HEAP_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            /* Don't advance - check if we can coalesce more */
        } else {
            block = block->next;
        }
    }
}

/*--- Allocate memory ---*/
void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size to 16 bytes */
    size = (size + 15) & ~15ULL;

    /* First-fit search */
    heap_block_t *block = heap_head;
    while (block) {
        if (block->magic != HEAP_MAGIC) {
            kernel_panic("Heap corruption detected!");
            return NULL;
        }
        if (block->free && block->size >= size) {
            /* Found a suitable block */
            heap_split(block, size);
            block->free = 0;
            heap_used += block->size;
            return (void *)((u8 *)block + HEAP_HEADER_SIZE);
        }
        block = block->next;
    }

    /* No suitable block found - try to expand heap */
    /* For Phase 1, just panic */
    kernel_panic("Heap: Out of memory!");
    return NULL;
}

void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t *block = (heap_block_t *)((u8 *)ptr - HEAP_HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) {
        kernel_panic("Heap: Invalid realloc pointer!");
        return NULL;
    }

    if (block->size >= new_size) {
        return ptr; /* Current block is large enough */
    }

    /* Allocate new block and copy */
    void *new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    heap_block_t *block = (heap_block_t *)((u8 *)ptr - HEAP_HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) {
        kernel_panic("Heap: Invalid free pointer (corrupted magic)!");
        return;
    }

    if (block->free) {
        serial_puts(SERIAL_COM1, "Heap: Double free detected!\n");
        return;
    }

    block->free = 1;
    heap_used -= block->size;

    /* Coalesce adjacent free blocks */
    heap_coalesce();
}

void *kmalloc_aligned(size_t size, size_t alignment) {
    /* Allocate extra space for alignment and header pointer */
    size_t total = size + alignment + sizeof(void *);
    void *raw = kmalloc(total);
    if (!raw) return NULL;

    /* Align the pointer */
    u64 addr = (u64)raw + sizeof(void *);
    u64 aligned = (addr + alignment - 1) & ~(alignment - 1);

    /* Store original pointer just before aligned address */
    *((void **)(aligned - sizeof(void *))) = raw;

    return (void *)aligned;
}

size_t heap_get_total(void) { return heap_total; }
size_t heap_get_used(void)  { return heap_used; }
size_t heap_get_free(void)  { return heap_total - heap_used; }