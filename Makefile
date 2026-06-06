CC      := gcc
LD      := ld
NASM    := nasm
OBJCOPY := objcopy

CFLAGS := -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -nostdlib -fno-asynchronous-unwind-tables \
          -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
          -Wall -Wextra -Isrc -O2

LDFLAGS := -m elf_i386 -T src/linker.ld -nostdlib

BUILD := build

SRCS    := $(wildcard src/*.c)
COBJS   := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
ASMSRCS := $(wildcard src/*.asm)
ASMOBJS := $(patsubst src/%.asm,$(BUILD)/%.o,$(ASMSRCS))

KERNEL_ELF := $(BUILD)/kernel.elf
KERNEL_BIN := $(BUILD)/kernel.bin
BOOT_BIN   := $(BUILD)/boot.bin
IMAGE      := $(BUILD)/orbit.img

QEMU       := qemu-system-i386
QEMUFLAGS  := -drive file=$(IMAGE),format=raw,if=ide -m 128M -no-reboot \
              -device isa-debug-exit,iobase=0xf4,iosize=0x04

all: $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(KERNEL_ELF): $(ASMOBJS) $(COBJS)
	$(LD) $(LDFLAGS) -o $@ $(ASMOBJS) $(COBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(BOOT_BIN): boot/boot.asm $(KERNEL_BIN) | $(BUILD)
	S=$$(( ( $$(stat -c%s $(KERNEL_BIN)) + 511 ) / 512 )); \
	$(NASM) -f bin -DKERNEL_SECTORS=$$S boot/boot.asm -o $@

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $@
	S=$$(( ( $$(stat -c%s $(KERNEL_BIN)) + 511 ) / 512 )); \
	truncate -s $$(( (1 + S) * 512 )) $@

run: $(IMAGE)
	$(QEMU) $(QEMUFLAGS) -display none -serial stdio

gui: $(IMAGE)
	$(QEMU) $(QEMUFLAGS) -serial stdio

clean:
	rm -rf $(BUILD)

.PHONY: all run gui clean
