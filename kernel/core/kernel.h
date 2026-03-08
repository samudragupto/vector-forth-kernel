#ifndef KERNEL_H
#define KERNEL_H

/*--- GCC freestanding headers (always available) ---*/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*--- Standard type aliases ---*/
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

/*--- Kernel version ---*/
#define VFK_VERSION_MAJOR   0
#define VFK_VERSION_MINOR   1
#define VFK_VERSION_PATCH   0
#define VFK_VERSION_STRING  "0.1.0"

/*--- Memory constants ---*/
#define KERNEL_PHYS_BASE    0x100000ULL
#define PAGE_SIZE           4096ULL

/*--- VGA constants ---*/
#define VGA_MEMORY          0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25

/*--- Port I/O ---*/
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

/*--- CPU control ---*/
static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

static inline u64 read_cr2(void) {
    u64 val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline u64 read_cr3(void) {
    u64 val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(u64 val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline void invlpg(void *addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

/*--- Kernel panic ---*/
void kernel_panic(const char *msg);

/*--- E820 memory map entry ---*/
typedef struct __attribute__((packed)) {
    u64 base;
    u64 length;
    u32 type;
    u32 acpi_ext;
} e820_entry_t;

#define E820_USABLE     1
#define E820_RESERVED   2

#endif /* KERNEL_H */