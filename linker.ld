/*=============================================================================
 * Linker Script - Higher-Half Kernel
 *
 * Virtual base:  0xFFFF_8000_0010_0000  (where code thinks it lives)
 * Physical load:             0x0010_0000  (where bootloader puts it)
 *=============================================================================*/

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFF800000000000;
KERNEL_PHYS_BASE = 0x100000;
KERNEL_VIRT_LOAD = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

SECTIONS
{
    /* Virtual address starts at higher-half + 1MB */
    . = KERNEL_VIRT_LOAD;

    /* Code section */
    .text ALIGN(4K) : AT(KERNEL_PHYS_BASE)
    {
        _kernel_start = .;
        *(.text.entry)   /* entry.asm first */
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(KERNEL_PHYS_BASE + (ADDR(.rodata) - KERNEL_VIRT_LOAD))
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    /* Initialized data */
    .data ALIGN(4K) : AT(KERNEL_PHYS_BASE + (ADDR(.data) - KERNEL_VIRT_LOAD))
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    /* Uninitialized data */
    .bss ALIGN(4K) : AT(KERNEL_PHYS_BASE + (ADDR(.bss) - KERNEL_VIRT_LOAD))
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}