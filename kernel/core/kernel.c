#include "kernel.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../interrupts/idt.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../memory/heap.h"
#include "../utils/string.h"
#include "../utils/stdlib.h"
#include "../../vm/core/forth.h"

extern void timer_init(u32 frequency);
extern void keyboard_init(void);

volatile u64 kernel_ticks = 0;

void kernel_panic(const char *msg) {
    cli();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();
    vga_puts("=== KERNEL PANIC ===\n");
    vga_puts(msg);
    vga_puts("\nSystem halted.");

    serial_puts(SERIAL_COM1, "\n!!! KERNEL PANIC: ");
    serial_puts(SERIAL_COM1, msg);
    serial_puts(SERIAL_COM1, "\n");

    while (1) { hlt(); }
}

void kernel_main(u64 mem_count, e820_entry_t *mem_map);

static void print_boot_info(u64 mem_count, e820_entry_t *mem_map) {
    char buf[32];

    vga_puts("Vector Forth Kernel v" VFK_VERSION_STRING "\n");
    vga_puts("========================================\n");

    vga_puts("kernel_main at: ");
    vga_put_hex((u64)&kernel_main);
    vga_putchar('\n');

    vga_puts("VGA buffer at:  ");
    vga_put_hex(VGA_MEMORY);
    vga_putchar('\n');

    serial_puts(SERIAL_COM1, "Vector Forth Kernel v" VFK_VERSION_STRING "\n");
    serial_puts(SERIAL_COM1, "kernel_main at: ");
    serial_put_hex(SERIAL_COM1, (u64)&kernel_main);
    serial_puts(SERIAL_COM1, "\n");

    vga_puts("Memory map entries: ");
    itoa((int)mem_count, buf, 10);
    vga_puts(buf);
    vga_putchar('\n');

    u64 total_usable = 0;
    for (u64 i = 0; i < mem_count; i++) {
        if (mem_map[i].type == E820_USABLE) {
            total_usable += mem_map[i].length;
        }
    }

    vga_puts("Total usable RAM: ");
    ultoa(total_usable / (1024 * 1024), buf, 10);
    vga_puts(buf);
    vga_puts(" MB\n");
}

__attribute__((section(".text.entry")))
void kernel_main(u64 mem_count, e820_entry_t *mem_map) {
    vga_init();
    serial_init(SERIAL_COM1);

    print_boot_info(mem_count, mem_map);

    vga_puts("[*] Initializing IDT...\n");
    idt_init();
    vga_puts("[+] IDT initialized\n");

    vga_puts("[*] Initializing PMM...\n");
    pmm_init(mem_map, (u32)mem_count);
    vga_puts("[+] PMM initialized\n");

    vga_puts("[*] Initializing VMM...\n");
    vmm_init();
    vga_puts("[+] VMM initialized\n");

    vga_puts("[*] Initializing heap...\n");
    heap_init();
    vga_puts("[+] Heap initialized\n");

    vga_puts("[*] Initializing timer (100 Hz)...\n");
    timer_init(100);
    vga_puts("[+] Timer initialized\n");

    vga_puts("[*] Initializing keyboard...\n");
    keyboard_init();
    vga_puts("[+] Keyboard initialized\n");

    vga_puts("[*] Enabling interrupts...\n");
    sti();
    vga_puts("[+] Interrupts enabled\n");

    vga_puts("\n========================================\n");
    vga_puts("Phase 1 Complete - Higher-Half Kernel\n");
    vga_puts("Entering Phase 2: Forth VM\n");
    vga_puts("========================================\n");

    serial_puts(SERIAL_COM1, "Starting Forth VM...\n");

    /* Initialize and run the Forth VM */
    forth_init();
    forth_run();

    /* Never reaches here */
    while (1) { hlt(); }
}