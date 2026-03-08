/*=============================================================================
 * Virtual Memory Manager implementation
 * Uses 4-level page tables (PML4 -> PDPT -> PD -> PT)
 *=============================================================================*/

#include "vmm.h"
#include "pmm.h"
#include "../utils/string.h"
#include "../drivers/serial.h"
#include "../core/kernel.h"

/*--- Get PML4 base from CR3 ---*/
static u64 *get_pml4(void) {
    return (u64 *)(read_cr3() & PTE_ADDR_MASK);
}

/*--- Page table index extraction from virtual address ---*/
static inline u64 pml4_index(u64 virt) { return (virt >> 39) & 0x1FF; }
static inline u64 pdpt_index(u64 virt) { return (virt >> 30) & 0x1FF; }
static inline u64 pd_index(u64 virt)   { return (virt >> 21) & 0x1FF; }
static inline u64 pt_index(u64 virt)   { return (virt >> 12) & 0x1FF; }

/*--- Get or create a page table level ---*/
static u64 *vmm_get_or_create_table(u64 *table, u64 index, u64 flags) {
    if (!(table[index] & PTE_PRESENT)) {
        /* Allocate a new page for the table */
        u64 phys = pmm_alloc_page();
        if (phys == 0) return NULL;

        /* Clear the new table page */
        memset((void *)phys, 0, PAGE_SIZE);

        /* Set the entry */
        table[index] = phys | flags | PTE_PRESENT | PTE_WRITABLE;
    }

    return (u64 *)(table[index] & PTE_ADDR_MASK);
}

/*--- Initialize VMM ---*/
void vmm_init(void) {
    /* Page tables are already set up by bootloader.
     * VMM provides interface for dynamic mapping.
     * We use identity mapping for physical memory access
     * since we're in early kernel with identity-mapped first 1GB.
     */
    serial_puts(SERIAL_COM1, "VMM: Initialized with bootloader page tables\n");
    serial_puts(SERIAL_COM1, "VMM: PML4 at ");
    serial_put_hex(SERIAL_COM1, read_cr3());
    serial_puts(SERIAL_COM1, "\n");
}

/*--- Map a single 4KB page ---*/
int vmm_map_page(u64 virt, u64 phys, u64 flags) {
    u64 *pml4 = get_pml4();

    /* Walk/create page table hierarchy */
    u64 *pdpt = vmm_get_or_create_table(pml4, pml4_index(virt),
                                        PTE_PRESENT | PTE_WRITABLE);
    if (!pdpt) return -1;

    u64 *pd = vmm_get_or_create_table(pdpt, pdpt_index(virt),
                                      PTE_PRESENT | PTE_WRITABLE);
    if (!pd) return -1;

    u64 *pt = vmm_get_or_create_table(pd, pd_index(virt),
                                      PTE_PRESENT | PTE_WRITABLE);
    if (!pt) return -1;

    /* Set the page table entry */
    u64 idx = pt_index(virt);
    pt[idx] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;

    /* Invalidate TLB for this page */
    invlpg((void *)virt);

    return 0;
}

/*--- Unmap a page ---*/
void vmm_unmap_page(u64 virt) {
    u64 *pml4 = get_pml4();

    u64 i4 = pml4_index(virt);
    if (!(pml4[i4] & PTE_PRESENT)) return;
    u64 *pdpt = (u64 *)(pml4[i4] & PTE_ADDR_MASK);

    u64 i3 = pdpt_index(virt);
    if (!(pdpt[i3] & PTE_PRESENT)) return;
    u64 *pd = (u64 *)(pdpt[i3] & PTE_ADDR_MASK);

    u64 i2 = pd_index(virt);
    if (!(pd[i2] & PTE_PRESENT)) return;

    /* Check if it's a 2MB page */
    if (pd[i2] & PTE_HUGE) {
        pd[i2] = 0;
        invlpg((void *)virt);
        return;
    }

    u64 *pt = (u64 *)(pd[i2] & PTE_ADDR_MASK);
    u64 i1 = pt_index(virt);

    pt[i1] = 0;
    invlpg((void *)virt);
}

/*--- Get physical address ---*/
u64 vmm_get_phys(u64 virt) {
    u64 *pml4 = get_pml4();

    u64 i4 = pml4_index(virt);
    if (!(pml4[i4] & PTE_PRESENT)) return (u64)-1;
    u64 *pdpt = (u64 *)(pml4[i4] & PTE_ADDR_MASK);

    u64 i3 = pdpt_index(virt);
    if (!(pdpt[i3] & PTE_PRESENT)) return (u64)-1;

    /* 1GB page? */
    if (pdpt[i3] & PTE_HUGE) {
        return (pdpt[i3] & PTE_ADDR_MASK) | (virt & 0x3FFFFFFF);
    }

    u64 *pd = (u64 *)(pdpt[i3] & PTE_ADDR_MASK);
    u64 i2 = pd_index(virt);
    if (!(pd[i2] & PTE_PRESENT)) return (u64)-1;

    /* 2MB page? */
    if (pd[i2] & PTE_HUGE) {
        return (pd[i2] & PTE_ADDR_MASK) | (virt & 0x1FFFFF);
    }

    u64 *pt = (u64 *)(pd[i2] & PTE_ADDR_MASK);
    u64 i1 = pt_index(virt);
    if (!(pt[i1] & PTE_PRESENT)) return (u64)-1;

    return (pt[i1] & PTE_ADDR_MASK) | (virt & 0xFFF);
}

/*--- Map a range ---*/
int vmm_map_range(u64 virt_start, u64 phys_start, u64 size, u64 flags) {
    u64 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (u64 i = 0; i < pages; i++) {
        int ret = vmm_map_page(
            virt_start + i * PAGE_SIZE,
            phys_start + i * PAGE_SIZE,
            flags
        );
        if (ret != 0) return ret;
    }
    return 0;
}

/*--- Check if mapped ---*/
int vmm_is_mapped(u64 virt) {
    return vmm_get_phys(virt) != (u64)-1;
}