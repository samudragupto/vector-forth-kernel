#=============================================================================
# Vector Forth Kernel - Build System (Phase 1-4)
#=============================================================================

ASM         = nasm
CC          = gcc
LD          = ld
OBJCOPY     = objcopy

BOOT_DIR    = boot
KERNEL_DIR  = kernel
VM_DIR      = vm
FS_DIR      = fs
BUILD_DIR   = build

GCC_INCLUDE = $(shell $(CC) -print-file-name=include)

ASMFLAGS    = -f elf64
CFLAGS      = -ffreestanding -nostdlib -nostdinc -isystem $(GCC_INCLUDE) \
              -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
              -mno-red-zone \
              -mcmodel=large -Wall -Wextra -Werror -O2 -g -std=c11 \
              -I kernel -I vm -I fs -I network

LDFLAGS     = -nostdlib -T linker.ld -z max-page-size=0x1000

BOOT_BIN    = $(BUILD_DIR)/boot.bin
STAGE2_BIN  = $(BUILD_DIR)/stage2.bin
KERNEL_ELF  = $(BUILD_DIR)/kernel.elf
KERNEL_BIN  = $(BUILD_DIR)/kernel.bin
OS_IMAGE    = $(BUILD_DIR)/forthos.img

# Tell Make where to search for .c and .asm files
VPATH = $(KERNEL_DIR)/core:$(KERNEL_DIR)/drivers:$(KERNEL_DIR)/interrupts:\
        $(KERNEL_DIR)/memory:$(KERNEL_DIR)/scheduler:$(KERNEL_DIR)/utils:\
        $(VM_DIR)/core:$(VM_DIR)/stack:$(VM_DIR)/dictionary:$(FS_DIR)\
		$(KERNEL_DIR)/drivers/pci:$(KERNEL_DIR)/drivers/net

# Phase 1-4 C sources (JUST filenames, no paths needed because of VPATH)
KERNEL_C_SRCS = \
    kernel.c vga.c serial.c timer.c keyboard.c ata.c \
	pci.c e1000.c \
    idt.c isr.c irq.c pmm.c vmm.c heap.c task.c \
    string.c stdlib.c stack.c dictionary.c forth.c \
    block_device.c filesystem.c avx.c

KERNEL_ASM_SRCS = \
    entry.asm switch.asm isr.asm

KERNEL_C_OBJS   = $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS = $(patsubst %.asm, $(BUILD_DIR)/%_asm.o, $(KERNEL_ASM_SRCS))
KERNEL_OBJS     = $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all clean run run-debug dirs

all: dirs $(OS_IMAGE)

dirs:
	@mkdir -p $(BUILD_DIR)

$(OS_IMAGE): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "[IMG] Creating disk image..."
	@dd if=/dev/zero of=$@ bs=512 count=20480 2>/dev/null
	@dd if=$(BOOT_BIN) of=$@ conv=notrunc bs=512 count=1 2>/dev/null
	@dd if=$(STAGE2_BIN) of=$@ conv=notrunc bs=512 seek=1 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$@ conv=notrunc bs=512 seek=34 2>/dev/null
	@echo "[OK] $(OS_IMAGE) created"

$(BOOT_BIN): $(BOOT_DIR)/boot.asm
	@echo "[ASM] $<"
	@$(ASM) -f bin -o $@ $<

$(STAGE2_BIN): $(BOOT_DIR)/long_mode.asm $(BOOT_DIR)/gdt.asm $(BOOT_DIR)/paging.asm
	@echo "[ASM] $<"
	@$(ASM) -f bin -I$(BOOT_DIR)/ -o $@ $<

$(KERNEL_ELF): $(KERNEL_OBJS)
	@echo "[LD]  $@"
	@$(LD) $(LDFLAGS) -o $@ $^

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@

# Unified C compile rule
$(BUILD_DIR)/%.o: %.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# Unified ASM compile rule
$(BUILD_DIR)/%_asm.o: %.asm
	@echo "[ASM] $<"
	@$(ASM) $(ASMFLAGS) -o $@ $<

# Run
run: $(OS_IMAGE)
	@echo "[RUN] Starting QEMU with AVX-128 Support..."
	@env -i DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY) PATH=/usr/bin:/bin \
		/usr/bin/qemu-system-x86_64 \
		-drive file=$(OS_IMAGE),format=raw \
		-netdev user,id=n0 \
		-device e1000,netdev=n0,mac=52:54:00:12:34:56 \
		-cpu max \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown

run-debug: $(OS_IMAGE)
	@echo "[DBG] Starting QEMU (GDB)..."
	@env -i DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY) PATH=/usr/bin:/bin \
		/usr/bin/qemu-system-x86_64 \
		-drive file=$(OS_IMAGE),format=raw \
		-netdev user,id=n0 \
		-device e1000,netdev=n0,mac=52:54:00:12:34:56 \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown \
		-s -S

clean:
	@rm -rf $(BUILD_DIR)/*
	@echo "[CLEAN] Done"