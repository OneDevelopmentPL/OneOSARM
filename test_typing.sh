#!/bin/bash
# Run QEMU and pipe keystrokes via mon:stdio
(sleep 2; echo "help") | qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M \
  -serial mon:stdio \
  -device bochs-display \
  -device virtio-keyboard-device \
  -device virtio-rng-device \
  -display none \
  -kernel oneos.bin
