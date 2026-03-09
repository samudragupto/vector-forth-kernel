#include "e1000.h"
#include "../pci/pci.h"
#include "../../memory/vmm.h"
#include "../../drivers/vga.h"
#include "../../drivers/serial.h"
#include "../../utils/stdlib.h"

/* The virtual address where we will map the NIC's hardware memory */
#define E1000_VIRT_BASE 0xFFFF8000F0000000ULL

/* Hardware Register Offsets */
#define REG_RAL 0x5400  /* Receive Address Low */
#define REG_RAH 0x5404  /* Receive Address High */

static u64 mmio_base = 0;
static u8  mac_address[6];

/* Helper to read 32-bit hardware register */
static u32 mmio_read32(u16 offset) {
    return *(volatile u32 *)(mmio_base + offset);
}

void e1000_init(void) {
    u32 phys_bar0 = pci_get_e1000_bar0();
    if (phys_bar0 == 0) return; /* No card found */

    /* The NIC's memory is at a random physical address (e.g., 0xFEB80000).
     * We MUST map it into our Higher-Half Virtual Memory using VMM to read it!
     * Map 128KB (32 pages) of MMIO space. */
    vmm_map_range(E1000_VIRT_BASE, phys_bar0, 128 * 1024, PTE_PRESENT | PTE_WRITABLE | PTE_NOCACHE);
    write_cr3(read_cr3()); /* Flush TLB */

    mmio_base = E1000_VIRT_BASE;

    /* Extract the MAC Address burnt into the hardware */
    u32 mac_low = mmio_read32(REG_RAL);
    u32 mac_high = mmio_read32(REG_RAH);

    mac_address[0] = (u8)(mac_low & 0xFF);
    mac_address[1] = (u8)((mac_low >> 8) & 0xFF);
    mac_address[2] = (u8)((mac_low >> 16) & 0xFF);
    mac_address[3] = (u8)((mac_low >> 24) & 0xFF);
    mac_address[4] = (u8)(mac_high & 0xFF);
    mac_address[5] = (u8)((mac_high >> 8) & 0xFF);

    /* Print the MAC Address */
    vga_puts("[+] NIC MAC Address: ");
    char buf[8];
    for (int i = 0; i < 6; i++) {
        utoa(mac_address[i], buf, 16);
        if (mac_address[i] < 16) vga_putchar('0');
        vga_puts(buf);
        if (i < 5) vga_putchar(':');
    }
    vga_putchar('\n');
}

u8* e1000_get_mac(void) {
    return mac_address;
}