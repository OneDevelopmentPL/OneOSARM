/* OneOS-ARM Virtio/Bochs GPU Driver Implementation */

#include "gpu.h"
#include "uart.h"

/* QEMU virt machine PCIe memory-mapped config space (ECAM) base address */
#define ECAM_BASE 0x4010000000ULL
/* Base address where we will map the PCI device's Memory BAR 0 */
#define PCI_MMIO_BASE 0x10000000

/* Bochs display default dims */
static int gpu_width = 1024;
static int gpu_height = 768;
static unsigned char *framebuffer = 0;

void gpu_init(void)
{
    uart_puts("Scanning PCI for Bochs Display...\n");

    /* Scan up to 32 PCI devices on bus 0 */
    for (int dev = 0; dev < 32; dev++) {
        volatile uint32_t *cfg = (volatile uint32_t *)((unsigned long long)ECAM_BASE | (dev << 15));
        
        /* Offset 0 is Vendor/Device ID */
        uint32_t vid_did = cfg[0];
        
        /* 0x11111234: Bochs Graphics Adapter (qemu-system -device bochs-display) */
        if (vid_did == 0x11111234) {
            uart_puts("-> Found Bochs Display!\n");
            
            /* Map BAR0 (framebuffer) to 16MB boundary */
            cfg[4] = PCI_MMIO_BASE; 
            /* Map BAR2 (MMIO registers) to +16MB just after the framebuffer */
            unsigned long long mmio_base = (unsigned long long)PCI_MMIO_BASE + 0x01000000;
            cfg[6] = (uint32_t)mmio_base;
            
            /* Enable Memory Space (bit 1) and Bus Master (bit 2) */
            cfg[1] = cfg[1] | 0x06;
            
            framebuffer = (unsigned char *)(unsigned long long)PCI_MMIO_BASE;
            
            uart_puts("   Framebuffer mapped at: 0x");
            uart_puthex((unsigned long long)framebuffer);
            uart_puts("\n   MMIO mapped at: 0x");
            uart_puthex(mmio_base);
            uart_puts("\n");

            /* Initialize Bochs VBE Display Registers via MMIO (offset 0x500) */
            volatile uint16_t *vbe = (volatile uint16_t *)(mmio_base + 0x500);

            /* Step 1: Disable display first */
            vbe[4] = 0x00;

            /* Step 2: Set mode parameters */
            vbe[1] = (uint16_t)gpu_width;   /* XRES */
            vbe[2] = (uint16_t)gpu_height;  /* YRES */
            vbe[3] = 32;                     /* BPP = 32bit (BGRX8888) */

            /* Step 3: Enable display with LFB */
            vbe[4] = 0x01 | 0x40;  /* VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED */

            uart_puts("   VBE Display Enabled (");
            uart_puthex(gpu_width);
            uart_puts("x");
            uart_puthex(gpu_height);
            uart_puts("x32)\n");
            return;
        }
    }
    
    uart_puts("-> Error: Bochs display not found!\n");
}

unsigned char* gpu_get_framebuffer(void)
{
    return framebuffer;
}

void gpu_get_dimensions(int *width, int *height)
{
    *width = gpu_width;
    *height = gpu_height;
}

void gpu_flush(void)
{
    /* Bochs display is linear memory; no flush required. */
}
