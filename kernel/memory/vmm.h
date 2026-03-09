#ifndef VMM_H
#define VMM_H

#include "../core/kernel.h"

#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITETHROUGH (1ULL << 3) /* <--- ADDED */
#define PTE_NOCACHE     (1ULL << 4)  /* <--- ADDED */
#define PTE_HUGE        (1ULL << 7)
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

void vmm_init(void);
int  vmm_map_page(u64 virt, u64 phys, u64 flags);
void vmm_unmap_page(u64 virt);
u64  vmm_get_phys(u64 virt);
int  vmm_map_range(u64 virt_start, u64 phys_start, u64 size, u64 flags);
int  vmm_is_mapped(u64 virt);

#endif