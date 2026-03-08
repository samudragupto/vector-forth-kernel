#include "pmm.h"
#include "../utils/string.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../utils/stdlib.h"

#define PMM_MAX_MEMORY      (4ULL * 1024 * 1024 * 1024)
#define PMM_BITMAP_SIZE     (PMM_MAX_MEMORY / PAGE_SIZE / 8)

/* Bitmap at higher-half virtual address */
static u8  *pmm_bitmap = (u8 *)PMM_BITMAP_VIRT;
static u64  pmm_total_pages = 0;
static u64  pmm_used_pages  = 0;
static u64  pmm_total_memory = 0;

static inline void bitmap_set(u64 page) {
    pmm_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(u64 page) {
    pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bitmap_test(u64 page) {
    return pmm_bitmap[page / 8] & (1 << (page % 8));
}

static u64 bitmap_find_free(void) {
    u64 bitmap_dwords = pmm_total_pages / 32;
    u32 *bitmap32 = (u32 *)pmm_bitmap;

    for (u64 i = 0; i < bitmap_dwords; i++) {
        if (bitmap32[i] != 0xFFFFFFFF) {
            for (int bit = 0; bit < 32; bit++) {
                u64 page = i * 32 + bit;
                if (page < pmm_total_pages && !bitmap_test(page)) {
                    return page;
                }
            }
        }
    }
    return (u64)-1;
}

static u64 bitmap_find_free_region(u64 count) {
    u64 consecutive = 0;
    u64 start = 0;

    for (u64 i = 0; i < pmm_total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start = i;
            consecutive++;
            if (consecutive >= count) return start;
        } else {
            consecutive = 0;
        }
    }
    return (u64)-1;
}

void pmm_init(e820_entry_t *mmap, u32 mmap_count) {
    pmm_total_memory = 0;
    for (u32 i = 0; i < mmap_count; i++) {
        u64 end = mmap[i].base + mmap[i].length;
        if (end > pmm_total_memory && mmap[i].type == E820_USABLE) {
            pmm_total_memory = end;
        }
    }

    if (pmm_total_memory > PMM_MAX_MEMORY) {
        pmm_total_memory = PMM_MAX_MEMORY;
    }

    pmm_total_pages = pmm_total_memory / PAGE_SIZE;

    u64 bitmap_bytes = pmm_total_pages / 8;
    if (pmm_total_pages % 8) bitmap_bytes++;

    /* Clear bitmap (mark all as used) */
    memset(pmm_bitmap, 0xFF, bitmap_bytes);
    pmm_used_pages = pmm_total_pages;

    /* Mark usable regions */
    for (u32 i = 0; i < mmap_count; i++) {
        if (mmap[i].type == E820_USABLE) {
            pmm_mark_region_free(mmap[i].base, mmap[i].length);
        }
    }

    /* Reserve critical regions */
    pmm_mark_region_used(0x0, 0x80000);          /* First 512KB (IVT, BDA, bootloader, page tables) */
    pmm_mark_region_used(0x100000, 0x100000);     /* Kernel code 1MB-2MB */
    
    u64 bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    pmm_mark_region_used(PMM_BITMAP_PHYS, bitmap_pages * PAGE_SIZE);

    char buf[32];
    serial_puts(SERIAL_COM1, "PMM: Total pages: ");
    ultoa(pmm_total_pages, buf, 10);
    serial_puts(SERIAL_COM1, buf);
    serial_puts(SERIAL_COM1, ", Free: ");
    ultoa(pmm_total_pages - pmm_used_pages, buf, 10);
    serial_puts(SERIAL_COM1, buf);
    serial_puts(SERIAL_COM1, "\n");
}

u64 pmm_alloc_page(void) {
    u64 page = bitmap_find_free();
    if (page == (u64)-1) {
        kernel_panic("PMM: Out of physical memory!");
        return 0;
    }
    bitmap_set(page);
    pmm_used_pages++;
    return page * PAGE_SIZE;
}

void pmm_free_page(u64 phys_addr) {
    u64 page = phys_addr / PAGE_SIZE;
    if (page >= pmm_total_pages) return;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        pmm_used_pages--;
    }
}

u64 pmm_alloc_pages(u64 count) {
    u64 start = bitmap_find_free_region(count);
    if (start == (u64)-1) {
        kernel_panic("PMM: Cannot allocate contiguous pages!");
        return 0;
    }
    for (u64 i = 0; i < count; i++) {
        bitmap_set(start + i);
        pmm_used_pages++;
    }
    return start * PAGE_SIZE;
}

void pmm_free_pages(u64 phys_addr, u64 count) {
    u64 start_page = phys_addr / PAGE_SIZE;
    for (u64 i = 0; i < count; i++) {
        u64 page = start_page + i;
        if (page < pmm_total_pages && bitmap_test(page)) {
            bitmap_clear(page);
            pmm_used_pages--;
        }
    }
}

void pmm_mark_region_used(u64 base, u64 length) {
    u64 start_page = base / PAGE_SIZE;
    u64 page_count = (length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (u64 i = 0; i < page_count; i++) {
        u64 page = start_page + i;
        if (page < pmm_total_pages && !bitmap_test(page)) {
            bitmap_set(page);
            pmm_used_pages++;
        }
    }
}

void pmm_mark_region_free(u64 base, u64 length) {
    u64 start_page = (base + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 end_page   = (base + length) / PAGE_SIZE;

    for (u64 page = start_page; page < end_page && page < pmm_total_pages; page++) {
        if (bitmap_test(page)) {
            bitmap_clear(page);
            pmm_used_pages--;
        }
    }
}

u64 pmm_get_total_memory(void)  { return pmm_total_memory; }
u64 pmm_get_free_memory(void)   { return (pmm_total_pages - pmm_used_pages) * PAGE_SIZE; }
u64 pmm_get_used_memory(void)   { return pmm_used_pages * PAGE_SIZE; }