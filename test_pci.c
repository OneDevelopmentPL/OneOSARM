/* test ECAM and bochs-display */
#include <stdint.h>

#define ECAM_BASE 0x3f000000
#define PCI_MMIO  0x10000000

// We pretend we are in the kernel
void pci_init_bochs(void) {
    for (int dev = 0; dev < 32; dev++) {
        volatile uint32_t *cfg = (volatile uint32_t *)(ECAM_BASE | (dev << 15));
        uint32_t vid_did = cfg[0];
        if (vid_did == 0x11111234) { // Bochs Display
            // Set BAR0 to PCI_MMIO
            cfg[4] = PCI_MMIO;
            // Enable Mem (Bit 1) and Bus Master (Bit 2) in Command Reg
            cfg[1] = cfg[1] | 0x06;
            // Now the framebuffer is at PCI_MMIO!
            return;
        }
    }
}
