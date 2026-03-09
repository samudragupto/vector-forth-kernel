#ifndef PCI_H
#define PCI_H

#include "../../core/kernel.h"

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* Read a 32-bit value from the PCI configuration space */
u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset);

/* Scan the bus and find the Intel E1000 Network Card */
void pci_init(void);

/* Get the E1000 memory address and IRQ */
u32  pci_get_e1000_bar0(void);
u8   pci_get_e1000_irq(void);

#endif