CC      := gcc
LD      := ld
NASM    := nasm
OBJCOPY := objcopy

CFLAGS := -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -fno-builtin -nostdlib -fno-asynchronous-unwind-tables \
          -mno-red-zone -mgeneral-regs-only -mcmodel=small \
          -Wall -Wextra -Isrc -Isrc/lib -Isrc/kernel/core -Isrc/kernel/arch/x86_64 \
          -Isrc/kernel/drivers -Isrc/kernel/memory -Isrc/kernel/net -Isrc/os \
          -Isrc/gui -Isrc/gui/apps -O2

LDFLAGS := -m elf_x86_64 -T src/kernel/core/linker.ld -nostdlib -z max-page-size=0x1000

BUILD := build

SRCS    := $(shell find src -name '*.c')
COBJS   := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
ASMSRCS := $(shell find src -name '*.asm')
ASMOBJS := $(patsubst src/%.asm,$(BUILD)/%.o,$(ASMSRCS))

KERNEL_ELF := $(BUILD)/kernel.elf
KERNEL_BIN := $(BUILD)/kernel.bin
BOOT_BIN   := $(BUILD)/boot.bin
IMAGE      := $(BUILD)/orbit.img

QEMU       := qemu-system-x86_64
QEMUFLAGS  := -drive file=$(IMAGE),format=raw,if=ide -m 1G -no-reboot \
              -vga std -global VGA.vgamem_mb=64 \
              -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
              -netdev user,id=net0 -device rtl8139,netdev=net0

all: $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.asm | $(BUILD)
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

$(KERNEL_ELF): $(ASMOBJS) $(COBJS)
	$(LD) $(LDFLAGS) -o $@ $(ASMOBJS) $(COBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(BOOT_BIN): boot/boot.asm $(KERNEL_BIN) | $(BUILD)
	S=$$(( ( $$(stat -c%s $(KERNEL_BIN)) + 511 ) / 512 )); \
	$(NASM) -f bin -DKERNEL_SECTORS=$$S boot/boot.asm -o $@

WALL_LBA := 8192
WALL_BIN  := assets/orbit_wallpaper.bin

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN) $(WALL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $@
	truncate -s $$(( $(WALL_LBA) * 512 )) $@
	cat $(WALL_BIN) >> $@

run: $(IMAGE)
	$(QEMU) $(QEMUFLAGS) -display none -serial stdio

gui: $(IMAGE)
	GDK_BACKEND=x11 $(QEMU) $(QEMUFLAGS) -display gtk,gl=off,show-cursor=off -serial stdio

fullscreen: $(IMAGE)
	GDK_BACKEND=x11 $(QEMU) $(QEMUFLAGS) -display gtk,gl=off,show-cursor=off -serial stdio -full-screen

logo:
	python3 scripts/genlogo.py assets/orbit_logo.png
	python3 scripts/png2logo.py assets/orbit_logo.png src/gui/logo_data.h orbit_logo

wallpaper:
	python3 scripts/genwall.py assets/orbit_wallpaper.png
	python3 scripts/png2bin.py assets/orbit_wallpaper.png assets/orbit_wallpaper.bin

assets: logo wallpaper

clean:
	rm -rf $(BUILD)

.PHONY: all run gui fullscreen logo wallpaper assets clean
