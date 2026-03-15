CC := aarch64-elf-gcc
OBJCOPY := aarch64-elf-objcopy
LD := aarch64-elf-ld

# Target specifications
CFLAGS := -Wall -Wextra -O2 -mgeneral-regs-only -ffreestanding -nostdinc -c
ASFLAGS := -c
LDFLAGS := -T linker.ld -nostdlib --nmagic

# Source files
SOURCES := boot.S uart.c kernel.c mem.c ds.c keyboard.c string.c terminal.c gpu.c fb.c graphics.c vfs.c gui.c virtio_input.c virtio_rng.c editor.c
OBJECTS := $(SOURCES:.S=.o)
OBJECTS := $(OBJECTS:.c=.o)

# Output binary
KERNEL := oneos.bin
ELF := oneos.elf
UIMAGE := oneos.uimage
BOOTSCR := boot.scr

# Default target
all: $(KERNEL) $(UIMAGE)

# Link the kernel
$(ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Convert ELF to binary
$(KERNEL): $(ELF)
	$(OBJCOPY) -O binary $< $@

# Create U-Boot image (uImage format)
$(UIMAGE): $(KERNEL)
	mkimage -A arm64 -T kernel -C none -O linux -a 0x40100000 -e 0x40100000 -n "OneOS-ARM 1" -d $(KERNEL) $@

# Compile boot script
$(BOOTSCR): boot.scr.txt
	mkimage -A arm64 -T script -C none -n "OneOS Boot Script" -d boot.scr.txt $@

# Compile assembly
%.o: %.S
	$(CC) $(ASFLAGS) -o $@ $<

# Compile C
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Run in QEMU with U-Boot
run: $(KERNEL)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M \
			-serial file:serial_log.txt \
			-device bochs-display \
			-device qemu-xhci \
			-device usb-tablet \
			-device virtio-keyboard-device \
			-device virtio-mouse-device \
			-device virtio-rng-device \
			-display cocoa \
			-kernel $(KERNEL)

# Run with serial attached to terminal (no GUI keyboard)
run-serial: $(KERNEL)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -serial mon:stdio -device bochs-display -kernel $(KERNEL)

# Run with serial console for UTM or testing
run-utm: $(KERNEL)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M \
		-serial mon:stdio \
		-device bochs-display \
		-device virtio-keyboard-device \
		-device virtio-rng-device \
		-display cocoa \
		-kernel $(KERNEL)

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(ELF) $(KERNEL) $(UIMAGE) $(BOOTSCR)

.PHONY: all run run-serial run-utm clean
