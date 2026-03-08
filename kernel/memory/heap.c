#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../utils/string.h"
#include "../drivers/serial.h"

typedef struct heap_block {
    u64                 size;
    struct heap_block  *next;
    u64                 magic;
    u8                  free;
    u8                  padding[7];
} __attribute__((packed)) heap_block_t;

#define HEAP_MAGIC      0xDEADBEEFCAFEBABEULL
#define HEAP_HEADER_SIZE sizeof(heap_block_t)
#define HEAP_MIN_BLOCK  64

/* Heap at higher-half virtual address */
#define HEAP_INITIAL_SIZE (1024 * 1024)
#define HEAP_MAX_SIZE   (16 * 1024 * 1024)

static heap_block_t *heap_head = NULL;
static u64           heap_end  = 0;
static size_t        heap_total = 0;
static size_t        heap_used  = 0;

void heap_init(void) {
    pmm_mark_region_used(HEAP_PHYS_START, HEAP_INITIAL_SIZE);

    /* Use virtual address for the heap */
    heap_head = (heap_block_t *)HEAP_VIRT_START;
    heap_head->size  = HEAP_INITIAL_SIZE - HEAP_HEADER_SIZE;
    heap_head->next  = NULL;
    heap_head->magic = HEAP_MAGIC;
    heap_head->free  = 1;

    heap_end   = HEAP_VIRT_START + HEAP_INITIAL_SIZE;
    heap_total = HEAP_INITIAL_SIZE;
    heap_used  = 0;

    serial_puts(SERIAL_COM1, "Heap: Initialized at ");
    serial_put_hex(SERIAL_COM1, HEAP_VIRT_START);
    serial_puts(SERIAL_COM1, ", size=1MB\n");
}

static void heap_split(heap_block_t *block, size_t size) {
    if (block->size <= size + HEAP_HEADER_SIZE + HEAP_MIN_BLOCK) {
        return;
    }

    heap_block_t *new_block = (heap_block_t *)((u8 *)block + HEAP_HEADER_SIZE + size);
    new_block->size  = block->size - size - HEAP_HEADER_SIZE;
    new_block->next  = block->next;
    new_block->magic = HEAP_MAGIC;
    new_block->free  = 1;

    block->size = size;
    block->next = new_block;
}

static void heap_coalesce(void) {
    heap_block_t *block = heap_head;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += HEAP_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = (size + 15) & ~15ULL;

    heap_block_t *block = heap_head;
    while (block) {
        if (block->magic != HEAP_MAGIC) {
            kernel_panic("Heap corruption detected!");
            return NULL;
        }
        if (block->free && block->size >= size) {
            heap_split(block, size);
            block->free = 0;
            heap_used += block->size;
            return (void *)((u8 *)block + HEAP_HEADER_SIZE);
        }
        block = block->next;
    }

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
        return ptr;
    }

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
        kernel_panic("Heap: Invalid free pointer!");
        return;
    }

    if (block->free) {
        serial_puts(SERIAL_COM1, "Heap: Double free detected!\n");
        return;
    }

    block->free = 1;
    heap_used -= block->size;

    heap_coalesce();
}

void *kmalloc_aligned(size_t size, size_t alignment) {
    size_t total = size + alignment + sizeof(void *);
    void *raw = kmalloc(total);
    if (!raw) return NULL;

    u64 addr = (u64)raw + sizeof(void *);
    u64 aligned = (addr + alignment - 1) & ~(alignment - 1);

    *((void **)(aligned - sizeof(void *))) = raw;

    return (void *)aligned;
}

size_t heap_get_total(void) { return heap_total; }
size_t heap_get_used(void)  { return heap_used; }
size_t heap_get_free(void)  { return heap_total - heap_used; }