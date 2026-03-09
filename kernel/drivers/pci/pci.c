#include "pci.h"
#include "../../drivers/serial.h"
#include "../../drivers/vga.h"
#include "../../utils/stdlib.h"

static u32 e1000_bar0 = 0;
static u8  e1000_irq  = 0;

u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 address;
    u32 lbus  = (u32)bus;
    u32 lslot = (u32)slot;
    u32 lfunc = (u32)func;
    u32 tmp = 0;

    /* Create configuration address */
    address = (u32)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((u32)0x80000000));

    /* Write out the address */
    outl(PCI_CONFIG_ADDRESS, address);
    /* Read in the data */
    tmp = inl(PCI_CONFIG_DATA);
    return tmp;
}

void pci_init(void) {
    serial_puts(SERIAL_COM1, "PCI: Scanning bus...\n");

    /* Brute-force scan bus 0 for devices */
    for (u16 slot = 0; slot < 32; slot++) {
        u32 vendor_device = pci_read_dword(0, slot, 0, 0);
        
        if ((vendor_device & 0xFFFF) == 0xFFFF) {
            continue; /* No device in this slot */
        }

        u16 vendor = vendor_device & 0xFFFF;
        u16 device = vendor_device >> 16;

        /* Check for Intel E1000 Network Card (Vendor: 0x8086, Device: 0x100E) */
        if (vendor == 0x8086 && device == 0x100E) {
            vga_puts("[+] Found Intel E1000 Network Card!\n");
            
            /* Read Base Address Register 0 (Memory Mapped I/O base) */
            u32 bar0 = pci_read_dword(0, slot, 0, 0x10);
            e1000_bar0 = bar0 & 0xFFFFFFF0; /* Mask out flags */
            
            /* Read Interrupt Line */
            u32 irq_reg = pci_read_dword(0, slot, 0, 0x3C);
            e1000_irq = irq_reg & 0xFF;

            char buf[16];
            vga_puts("    -> MMIO Base: 0x");
            ultoa(e1000_bar0, buf, 16);
            vga_puts(buf);
            vga_puts("\n    -> IRQ Line:  ");
            itoa(e1000_irq, buf, 10);
            vga_puts(buf);
            vga_putchar('\n');
            return;
        }
    }
    vga_puts("[-] Intel E1000 not found.\n");
}

u32 pci_get_e1000_bar0(void) { return e1000_bar0; }
u8  pci_get_e1000_irq(void)  { return e1000_irq; }