#=============================================================================
# Vector Forth Kernel - Build System (Phase 1 + 2)
#=============================================================================

ASM         = nasm
CC          = gcc
LD          = ld
OBJCOPY     = objcopy

BOOT_DIR    = boot
KERNEL_DIR  = kernel
VM_DIR      = vm
BUILD_DIR   = build

GCC_INCLUDE = $(shell $(CC) -print-file-name=include)

ASMFLAGS    = -f elf64
CFLAGS      = -ffreestanding \
              -nostdlib \
              -nostdinc \
              -isystem $(GCC_INCLUDE) \
              -fno-builtin \
              -fno-stack-protector \
              -fno-pic \
              -fno-pie \
              -mno-red-zone \
              -mno-mmx -mno-sse -mno-sse2 \
              -mcmodel=large \
              -Wall -Wextra -Werror \
              -O2 -g \
              -std=c11 \
              -I kernel
LDFLAGS     = -nostdlib -T linker.ld -z max-page-size=0x1000

BOOT_BIN    = $(BUILD_DIR)/boot.bin
STAGE2_BIN  = $(BUILD_DIR)/stage2.bin
KERNEL_ELF  = $(BUILD_DIR)/kernel.elf
KERNEL_BIN  = $(BUILD_DIR)/kernel.bin
OS_IMAGE    = $(BUILD_DIR)/forthos.img

# Phase 1 + Phase 2 C sources
KERNEL_C_SRCS = \
    $(KERNEL_DIR)/core/kernel.c \
    $(KERNEL_DIR)/drivers/vga.c \
    $(KERNEL_DIR)/drivers/serial.c \
    $(KERNEL_DIR)/drivers/timer.c \
    $(KERNEL_DIR)/drivers/keyboard.c \
    $(KERNEL_DIR)/interrupts/idt.c \
    $(KERNEL_DIR)/interrupts/isr.c \
    $(KERNEL_DIR)/interrupts/irq.c \
    $(KERNEL_DIR)/memory/pmm.c \
    $(KERNEL_DIR)/memory/vmm.c \
    $(KERNEL_DIR)/memory/heap.c \
    $(KERNEL_DIR)/utils/string.c \
    $(KERNEL_DIR)/utils/stdlib.c \
    $(VM_DIR)/stack/stack.c \
    $(VM_DIR)/dictionary/dictionary.c \
    $(VM_DIR)/core/forth.c

KERNEL_ASM_SRCS = \
    $(KERNEL_DIR)/core/entry.asm \
    $(KERNEL_DIR)/interrupts/isr.asm

KERNEL_C_OBJS   = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(KERNEL_C_SRCS)))
KERNEL_ASM_OBJS = $(patsubst %.asm, $(BUILD_DIR)/%_asm.o, $(notdir $(KERNEL_ASM_SRCS)))
KERNEL_OBJS     = $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all clean run run-debug dirs

all: dirs $(OS_IMAGE)

dirs:
	@mkdir -p $(BUILD_DIR)

$(OS_IMAGE): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "[IMG] Creating disk image..."
	@dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
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

# C compile rules
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/core/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/drivers/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/interrupts/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/memory/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/utils/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(VM_DIR)/stack/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(VM_DIR)/dictionary/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(VM_DIR)/core/%.c
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# ASM compile rules
$(BUILD_DIR)/%_asm.o: $(KERNEL_DIR)/core/%.asm
	@echo "[ASM] $<"
	@$(ASM) $(ASMFLAGS) -o $@ $<

$(BUILD_DIR)/%_asm.o: $(KERNEL_DIR)/interrupts/%.asm
	@echo "[ASM] $<"
	@$(ASM) $(ASMFLAGS) -o $@ $<

# Run
run: $(OS_IMAGE)
	@echo "[RUN] Starting QEMU..."
	@env -i DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY) PATH=/usr/bin:/bin \
		/usr/bin/qemu-system-x86_64 \
		-drive file=$(OS_IMAGE),format=raw \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown

run-debug: $(OS_IMAGE)
	@echo "[DBG] Starting QEMU (GDB)..."
	@env -i DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY) PATH=/usr/bin:/bin \
		/usr/bin/qemu-system-x86_64 \
		-drive file=$(OS_IMAGE),format=raw \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown \
		-s -S

clean:
	@rm -rf $(BUILD_DIR)/*
	@echo "[CLEAN] Done"