CC      := gcc
LD      := ld
NASM    := nasm
OBJCOPY := objcopy

CFLAGS := -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -nostdlib -fno-asynchronous-unwind-tables \
          -mno-red-zone -mgeneral-regs-only -mcmodel=small \
          -Wall -Wextra -Isrc -O2

LDFLAGS := -m elf_x86_64 -T src/linker.ld -nostdlib -z max-page-size=0x1000

BUILD := build

SRCS    := $(wildcard src/*.c)
COBJS   := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
ASMSRCS := $(wildcard src/*.asm)
ASMOBJS := $(patsubst src/%.asm,$(BUILD)/%.o,$(ASMSRCS))

KERNEL_ELF := $(BUILD)/kernel.elf
KERNEL_BIN := $(BUILD)/kernel.bin
BOOT_BIN   := $(BUILD)/boot.bin
IMAGE      := $(BUILD)/orbit.img

QEMU       := qemu-system-x86_64
QEMUFLAGS  := -drive file=$(IMAGE),format=raw,if=ide -m 256M -no-reboot -vga std \
              -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
              -netdev user,id=net0 -device rtl8139,netdev=net0

all: $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

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

fullscreen: $(IMAGE)
	$(QEMU) $(QEMUFLAGS) -serial stdio -full-screen

clean:
	rm -rf $(BUILD)

.PHONY: all run gui fullscreen clean
