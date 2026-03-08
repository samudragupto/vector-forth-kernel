#ifndef PMM_H
#define PMM_H

#include "../core/kernel.h"

void pmm_init(e820_entry_t *mmap, u32 mmap_count);
u64  pmm_alloc_page(void);
void pmm_free_page(u64 phys_addr);
u64  pmm_alloc_pages(u64 count);
void pmm_free_pages(u64 phys_addr, u64 count);
u64  pmm_get_total_memory(void);
u64  pmm_get_free_memory(void);
u64  pmm_get_used_memory(void);
void pmm_mark_region_used(u64 base, u64 length);
void pmm_mark_region_free(u64 base, u64 length);

#endif