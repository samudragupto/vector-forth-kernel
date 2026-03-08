/*=============================================================================
 * PIT (Programmable Interval Timer) Driver - IRQ0
 *=============================================================================*/

#include "../core/kernel.h"
#include "vga.h"
#include "serial.h"

#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43
#define PIT_FREQUENCY   1193182     /* Base PIT frequency in Hz */

extern volatile u64 kernel_ticks;

/*--- IRQ0 handler (called from irq.c) ---*/
void timer_handler_c(void) {
    kernel_ticks++;

    /* Optionally display tick count at top-right corner */
    /* (Uncomment for debug)
    char buf[12];
    ultoa(kernel_ticks, buf, 10);
    int len = 0;
    while (buf[len]) len++;
    for (int i = 0; i < len; i++) {
        vga_put_at(VGA_WIDTH - len + i, 0, buf[i], 0x0E);
    }
    */
}

/*--- Initialize PIT at given frequency ---*/
void timer_init(u32 frequency) {
    if (frequency == 0) frequency = 100;

    u32 divisor = PIT_FREQUENCY / frequency;

    /* Send command: Channel 0, lobyte/hibyte, rate generator */
    outb(PIT_COMMAND, 0x36);

    /* Send divisor */
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));
}

/*--- Get tick count ---*/
u64 timer_get_ticks(void) {
    return kernel_ticks;
}

/*--- Simple delay (approximate) ---*/
void timer_wait(u64 ticks) {
    u64 target = kernel_ticks + ticks;
    while (kernel_ticks < target) {
        hlt();
    }
}