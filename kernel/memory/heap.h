#ifndef HEAP_H
#define HEAP_H

#include "../core/kernel.h"

void  heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);
void *kmalloc_aligned(size_t size, size_t alignment);
size_t heap_get_total(void);
size_t heap_get_used(void);
size_t heap_get_free(void);

#endif