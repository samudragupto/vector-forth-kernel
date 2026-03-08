#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

#define VFK_VERSION_MAJOR   0
#define VFK_VERSION_MINOR   1
#define VFK_VERSION_PATCH   0
#define VFK_VERSION_STRING  "0.1.0"

/*--- Higher-Half Virtual Memory Layout ---*/
#define KERNEL_VIRT_BASE        0xFFFF800000000000ULL
#define KERNEL_PHYS_BASE        0x100000ULL

/*--- Convert physical to virtual and back ---*/
#define PHYS_TO_VIRT(addr)      ((u64)(addr) + KERNEL_VIRT_BASE)
#define VIRT_TO_PHYS(addr)      ((u64)(addr) - KERNEL_VIRT_BASE)

/*--- VGA is at physical 0xB8000, mapped to higher-half ---*/
#define VGA_PHYS_MEMORY         0xB8000ULL
#define VGA_MEMORY              (KERNEL_VIRT_BASE + VGA_PHYS_MEMORY)
#define VGA_WIDTH               80
#define VGA_HEIGHT              25

/*--- Memory regions (all in higher-half virtual space) ---*/
#define PMM_BITMAP_PHYS         0x200000ULL
#define PMM_BITMAP_VIRT         (KERNEL_VIRT_BASE + PMM_BITMAP_PHYS)

#define HEAP_PHYS_START         0x300000ULL
#define HEAP_VIRT_START         (KERNEL_VIRT_BASE + HEAP_PHYS_START)

#define PAGE_SIZE               4096ULL

/*--- Port I/O (unchanged, these use CPU ports not memory) ---*/
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

void kernel_panic(const char *msg);

typedef struct __attribute__((packed)) {
    u64 base;
    u64 length;
    u32 type;
    u32 acpi_ext;
} e820_entry_t;

#define E820_USABLE     1
#define E820_RESERVED   2

#endif