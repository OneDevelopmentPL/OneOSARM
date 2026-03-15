/* OneOS-ARM Virtio-Entropy (RNG) Driver Implementation
 *
 * Scans for virtio-rng devices, sets up a virtqueue, and requests
 * random data from the host via the device.
 */

#include "virtio_rng.h"
#include "uart.h"

#define VIRTIO_MMIO_BASE   0x0a000000UL
#define VIRTIO_MMIO_STRIDE 0x200
#define VIRTIO_MMIO_COUNT  32
#define VIRTIO_MAGIC 0x74726976

#define VIRT_QUEUE_SIZE 8
#define VIRTQ_ALIGN 4096

struct vring_desc {
    unsigned long long addr;
    unsigned int       len;
    unsigned short     flags;
    unsigned short     next;
};

struct vring_avail {
    unsigned short flags;
    unsigned short idx;
    unsigned short ring[VIRT_QUEUE_SIZE];
};

struct vring_used_elem {
    unsigned int id;
    unsigned int len;
};

struct vring_used {
    unsigned short           flags;
    unsigned short           idx;
    struct vring_used_elem   ring[VIRT_QUEUE_SIZE];
};

struct virtq {
    struct vring_desc  desc[VIRT_QUEUE_SIZE];
    struct vring_avail avail;
    unsigned char      pad[VIRTQ_ALIGN - ((sizeof(struct vring_desc)*VIRT_QUEUE_SIZE + sizeof(struct vring_avail)) % VIRTQ_ALIGN)];
    struct vring_used  used;
} __attribute__((aligned(4096)));

typedef struct {
    volatile unsigned int *mmio;
    struct virtq       vq;
    unsigned short last_used_idx;
} rng_dev_t;

static rng_dev_t rng_dev;
static int rng_ready = 0;

static void mmio_write(volatile unsigned int *base, unsigned int off, unsigned int val) {
    base[off / 4] = val;
}

static unsigned int mmio_read(volatile unsigned int *base, unsigned int off) {
    return base[off / 4];
}

void virtio_rng_init(void) {
    for (int i = 0; i < VIRTIO_MMIO_COUNT; i++) {
        volatile unsigned int *mmio = (volatile unsigned int *)(VIRTIO_MMIO_BASE + (unsigned long long)i * VIRTIO_MMIO_STRIDE);
        if (mmio_read(mmio, VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) continue;
        if (mmio_read(mmio, VIRTIO_MMIO_DEVICE_ID) != VIRTIO_ID_RNG) continue;

        uart_puts("VirtIO RNG Device found at index ");
        uart_puthex(i);
        uart_puts("\n");

        /* Reset and Setup */
        mmio_write(mmio, VIRTIO_MMIO_STATUS, 0);
        mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
        mmio_write(mmio, VIRTIO_MMIO_DRIVER_FEATURES, 0);
        mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

        mmio_write(mmio, VIRTIO_MMIO_QUEUE_SEL, 0);
        mmio_write(mmio, VIRTIO_MMIO_GUEST_PAGE_SIZE, VIRTQ_ALIGN);
        mmio_write(mmio, VIRTIO_MMIO_QUEUE_NUM, VIRT_QUEUE_SIZE);
        mmio_write(mmio, VIRTIO_MMIO_QUEUE_ALIGN, VIRTQ_ALIGN);

        /* Clear VQ memory */
        unsigned char *ptr = (unsigned char *)&rng_dev.vq;
        for (unsigned int j = 0; j < sizeof(struct virtq); j++) ptr[j] = 0;

        unsigned int pfn = ((unsigned int)(unsigned long)&rng_dev.vq) / VIRTQ_ALIGN;
        mmio_write(mmio, VIRTIO_MMIO_QUEUE_PFN, pfn);

        mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

        rng_dev.mmio = mmio;
        rng_dev.last_used_idx = 0;
        rng_ready = 1;
        return;
    }
}

int virtio_rng_get_bytes(void *buf, int len) {
    if (!rng_ready) return 0;
    if (len > 4096) len = 4096; /* Limit request size */

    rng_dev.vq.desc[0].addr = (unsigned long long)buf;
    rng_dev.vq.desc[0].len = len;
    rng_dev.vq.desc[0].flags = 2; /* VRING_DESC_F_WRITE */
    rng_dev.vq.desc[0].next = 0;

    rng_dev.vq.avail.ring[rng_dev.vq.avail.idx % VIRT_QUEUE_SIZE] = 0;
    asm volatile("dmb sy" ::: "memory");
    rng_dev.vq.avail.idx++;
    asm volatile("dmb sy" ::: "memory");

    mmio_write(rng_dev.mmio, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    /* Poll for completion (bare-metal style) */
    while (rng_dev.last_used_idx == rng_dev.vq.used.idx) {
        asm volatile("nop");
    }

    int bytes_read = rng_dev.vq.used.ring[rng_dev.last_used_idx % VIRT_QUEUE_SIZE].len;
    rng_dev.last_used_idx++;
    return bytes_read;
}

unsigned int virtio_rng_rand(void) {
    unsigned int val = 0;
    virtio_rng_get_bytes(&val, sizeof(val));
    return val;
}
