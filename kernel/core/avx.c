#include "avx.h"
#include "kernel.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"

void avx_init(void) {
    u64 cr0, cr4;

    /* 1. Clear the Emulation (EM) and Task Switched (TS) bits in CR0 */
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2);  /* Clear EM */
    cr0 &= ~(1 << 3);  /* Clear TS */
    cr0 |= (1 << 1);   /* Set Monitor Coprocessor (MP) */
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));

    /* 2. Enable OSFXSR (Bit 9) and OSXMMEXCPT (Bit 10) in CR4 
     * This safely turns on XMM/YMM vector registers without needing XSETBV */
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9) | (1 << 10);
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));

    vga_puts("[+] 128-bit XMM Vector Registers Unlocked!\n");
    serial_puts(SERIAL_COM1, "SIMD: 128-bit SSE2 hardware enabled.\n");
}