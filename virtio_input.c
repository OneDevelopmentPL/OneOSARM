/* OneOS-ARM Virtio-Input Driver Implementation
 *
 * Implements a minimal virtio-mmio input driver for QEMU's virtio-keyboard
 * and virtio-mouse devices. Scans virtio-mmio buses, finds input devices,
 * sets up a virtqueue, and polls for events.
 *
 * Linux input events are 8 bytes: type(u16), code(u16), value(s32)
 */

#include "virtio_input.h"
#include "uart.h"

/* Virtio-mmio base and stride */
#define VIRTIO_MMIO_BASE   0x0a000000UL
#define VIRTIO_MMIO_STRIDE 0x200
#define VIRTIO_MMIO_COUNT  32

/* Virtio magic number */
#define VIRTIO_MAGIC 0x74726976

/* Virtio-Input Config structure offset (Device specific configuration) */
#define VIRTIO_MMIO_CONFIG 0x100

/* Queue size — must be power of 2 */
#define VIRT_QUEUE_SIZE 16

/* -------- Virtqueue structures (virtio 1.0 split queue) ---------- */
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

/* -------- Virtqueue Legacy Contiguous Layout ------------------ */
#define VIRTQ_ALIGN 4096

struct virtq {
    struct vring_desc  desc[VIRT_QUEUE_SIZE];
    struct vring_avail avail;
    unsigned char      pad[VIRTQ_ALIGN -
                            ((sizeof(struct vring_desc)*VIRT_QUEUE_SIZE + sizeof(struct vring_avail)) % VIRTQ_ALIGN)];
    struct vring_used  used;
} __attribute__((aligned(4096)));

/* ABS codes (from Linux input-event-codes.h) */
#define ABS_X    0x00
#define ABS_Y    0x01
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111

/* -------- Virtio input event ------------------------------------ */
struct virtio_input_event {
    unsigned short type;
    unsigned short code;
    unsigned int   value;
};

/* -------- Per-device state ------------------------------------- */
#define MAX_INPUT_DEVS 4

typedef struct {
    volatile unsigned int *mmio;
    struct virtq       vq;
    struct virtio_input_event events[VIRT_QUEUE_SIZE];
    unsigned short last_used_idx;
    int is_mouse;    /* 0 = keyboard, 1 = relative mouse, 2 = absolute mouse */
} input_dev_t;

static input_dev_t devs[MAX_INPUT_DEVS] __attribute__((aligned(4096)));
static int dev_count = 0;

/* Pending keyboard char and mouse state */
static unsigned char pending_key = 0;
static int           pending_dx  = 0;
static int           pending_dy  = 0;
static int           pending_btn = 0;
static int           pending_btn_state = 0; /* held state for button */

/* Absolute mouse position tracking */
static int abs_x = -1, abs_y = -1;
#define ABS_SCALE_X 1024  /* Screen width for abs->pixel mapping */
#define ABS_SCALE_Y 768   /* Screen height for abs->pixel mapping */
static int abs_max_x = 32767;  /* Default ABS range from QEMU */
static int abs_max_y = 32767;
static int got_abs_event = 0;

/* Modifier and special key tracking */
static int pending_modifiers = 0;
static int pending_arrow = 0;

/* -------- Linux keycode -> ASCII table (US QWERTY) -------------- */
static const unsigned char keymap[128] = {
    0,   0,   '1', '2', '3', '4', '5', '6',  /* 0-7   */
    '7', '8', '9', '0', '-', '=', '\b', '\t', /* 8-15  */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  /* 16-23 */
    'o', 'p', '[', ']', '\r', 0,  'a', 's',  /* 24-31 */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 32-39 */
    '\'','`', 0,  '\\','z', 'x', 'c', 'v',   /* 40-47 */
    'b', 'n', 'm', ',', '.', '/', 0,   '*',  /* 48-55 */
    0,   ' ', 0,   0,   0,   0,   0,   0,    /* 56-63 */
    0,   0,   0,   0,   0,   0,   0,   '7',  /* 64-71 */
    '8', '9', '-', '4', '5', '6', '+', '1',  /* 72-79 */
    '2', '3', '0', '.', 0,   0,   0,   0,    /* 80-87 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 88-95 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 96-103 */
    0,   0,   0,   0,'\r', 0, 0,   0,        /* 104-111 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 112-119 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 120-127 */
};

/* -------- Helper write/read virtio-mmio register -------------- */
static void mmio_write(volatile unsigned int *base, unsigned int off, unsigned int val)
{
    base[off / 4] = val;
}

static unsigned int mmio_read(volatile unsigned int *base, unsigned int off)
{
    return base[off / 4];
}

/* -------- Detect device type via config space query ------------ */
/* VIRTIO_INPUT_CFG_EV_BITS select=0x11, subsel=EV_REL(2) or EV_ABS(3)
 * If the device reports bits for EV_REL, it's a relative mouse.
 * If it reports bits for EV_ABS, it's an absolute pointer.
 * If neither, it's a keyboard.
 *
 * Config space layout at VIRTIO_MMIO_CONFIG:
 *   byte  0: select
 *   byte  1: subsel
 *   byte  2: size  (read-back: how many bytes of data follow)
 *   bytes 8+: data
 */
static int detect_device_type(volatile unsigned int *mmio)
{
    volatile unsigned char *cfg = (volatile unsigned char *)mmio + VIRTIO_MMIO_CONFIG;

    /* Check for EV_REL support (relative mouse) */
    cfg[0] = 0x11;  /* VIRTIO_INPUT_CFG_EV_BITS */
    cfg[1] = EV_REL; /* subsel = EV_REL */
    asm volatile("dmb sy" ::: "memory");
    for (volatile int k = 0; k < 2000; k++);
    asm volatile("dmb sy" ::: "memory");
    unsigned char rel_size = cfg[2];

    if (rel_size > 0) {
        uart_puts("  -> EV_REL supported (relative mouse)\n");
        return 1; /* relative mouse */
    }

    /* Check for EV_ABS support (absolute pointer / tablet) */
    cfg[0] = 0x11;  /* VIRTIO_INPUT_CFG_EV_BITS */
    cfg[1] = EV_ABS; /* subsel = EV_ABS */
    asm volatile("dmb sy" ::: "memory");
    for (volatile int k = 0; k < 2000; k++);
    asm volatile("dmb sy" ::: "memory");
    unsigned char abs_size = cfg[2];

    if (abs_size > 0) {
        uart_puts("  -> EV_ABS supported (absolute pointer)\n");
        return 2; /* absolute mouse */
    }

    /* Neither -> keyboard */
    return 0;
}

/* -------- Initialize a single virtio-input device ------------- */
static int init_one_device(volatile unsigned int *mmio, input_dev_t *dev)
{
    /* 1. Reset device */
    mmio_write(mmio, VIRTIO_MMIO_STATUS, 0);

    /* 2. ACK and DRIVER */
    mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* 3. Features */
    mmio_write(mmio, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    /* 4. Detect device type */
    int dtype = detect_device_type(mmio);

    /* 5. Setup Queue 0 */
    mmio_write(mmio, VIRTIO_MMIO_QUEUE_SEL, 0);
    unsigned int max_num = mmio_read(mmio, VIRTIO_MMIO_QUEUE_NUM_MAX);
    unsigned int q_size = (VIRT_QUEUE_SIZE <= max_num) ? VIRT_QUEUE_SIZE : max_num;

    mmio_write(mmio, VIRTIO_MMIO_GUEST_PAGE_SIZE, VIRTQ_ALIGN);
    mmio_write(mmio, VIRTIO_MMIO_QUEUE_NUM, q_size);
    mmio_write(mmio, VIRTIO_MMIO_QUEUE_ALIGN, VIRTQ_ALIGN);

    /* Clear and prepare descriptors */
    unsigned char *ptr = (unsigned char *)&dev->vq;
    for (int i = 0; i < (int)sizeof(struct virtq); i++) ptr[i] = 0;

    for (int i = 0; i < (int)q_size; i++) {
        dev->vq.desc[i].addr = (unsigned long long)(void *)&dev->events[i];
        dev->vq.desc[i].len  = sizeof(struct virtio_input_event);
        dev->vq.desc[i].flags = 2; /* WRITE */
        dev->vq.avail.ring[i] = (unsigned short)i;
    }
    asm volatile("dmb sy" ::: "memory");
    dev->vq.avail.idx = q_size;

    /* Write PFN */
    unsigned int pfn = ((unsigned int)(unsigned long)&dev->vq) / VIRTQ_ALIGN;
    mmio_write(mmio, VIRTIO_MMIO_QUEUE_PFN, pfn);

    /* 6. Driver OK */
    mmio_write(mmio, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    /* Notify device */
    mmio_write(mmio, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    /* Log */
    uart_puts("  Type: ");
    if (dtype == 0) uart_puts("[KBD]");
    else if (dtype == 1) uart_puts("[MOUSE-REL]");
    else uart_puts("[MOUSE-ABS]");
    uart_puts("\n");

    dev->mmio = mmio;
    dev->last_used_idx = 0;
    dev->is_mouse = dtype;
    return 1;
}

/* -------- Scan and initialize virtio-input devices ------------ */
void virtio_input_init(void)
{
    for (int i = 0; i < VIRTIO_MMIO_COUNT && dev_count < MAX_INPUT_DEVS; i++) {
        volatile unsigned int *mmio = (volatile unsigned int *)
            (VIRTIO_MMIO_BASE + (unsigned long long)i * VIRTIO_MMIO_STRIDE);

        unsigned int magic = mmio_read(mmio, VIRTIO_MMIO_MAGIC_VALUE);
        if (magic != VIRTIO_MAGIC) continue;

        unsigned int device_id = mmio_read(mmio, VIRTIO_MMIO_DEVICE_ID);
        if (device_id != VIRTIO_ID_INPUT) continue;

        uart_puts("VirtIO Input Device found at index ");
        uart_puthex(i);
        uart_puts("\n");

        if (init_one_device(mmio, &devs[dev_count])) {
            dev_count++;
        }
    }

    uart_puts("VirtIO Input: ");
    uart_puthex(dev_count);
    uart_puts(" device(s) initialized\n");
}

/* -------- Poll events for all devices ------------------------- */
static void poll_events(void)
{
    for (int d = 0; d < dev_count; d++) {
        input_dev_t *dev = &devs[d];
        volatile unsigned int *mmio = dev->mmio;

        /* Acknowledge and clear interrupt */
        unsigned int isr = mmio_read(mmio, VIRTIO_MMIO_INTERRUPT_STATUS);
        if (isr) {
            mmio_write(mmio, VIRTIO_MMIO_INTERRUPT_ACK, isr);
        }

        asm volatile("dmb sy" ::: "memory");

        /* Drain the used ring */
        while (dev->last_used_idx != dev->vq.used.idx) {
            asm volatile("dmb sy" ::: "memory");
            unsigned int idx = dev->last_used_idx % VIRT_QUEUE_SIZE;
            unsigned int desc_idx = dev->vq.used.ring[idx].id;
            struct virtio_input_event *evt = &dev->events[desc_idx];

            unsigned short type = evt->type;
            unsigned short code = evt->code;
            unsigned int   val  = evt->value;

            if (dev->is_mouse == 1) {
                /* Relative mouse */
                if (type == EV_REL) {
                    if (code == REL_X) { pending_dx += (int)val; }
                    else if (code == REL_Y) { pending_dy += (int)val; }
                } else if (type == EV_KEY) {
                    if (code == BTN_LEFT) {
                        pending_btn_state = (int)val;
                        if (val == 1) pending_btn |= 1;
                        else pending_btn &= ~1;
                    } else if (code == BTN_RIGHT) {
                        if (val == 1) pending_btn |= 2;
                        else pending_btn &= ~2;
                    }
                }
            } else if (dev->is_mouse == 2) {
                /* Absolute mouse (usb-tablet style) */
                if (type == EV_ABS) {
                    if (code == ABS_X) {
                        int new_x = (int)val * ABS_SCALE_X / abs_max_x;
                        if (abs_x >= 0) {
                            pending_dx += new_x - abs_x;
                        }
                        abs_x = new_x;
                        got_abs_event = 1;
                    } else if (code == ABS_Y) {
                        int new_y = (int)val * ABS_SCALE_Y / abs_max_y;
                        if (abs_y >= 0) {
                            pending_dy += new_y - abs_y;
                        }
                        abs_y = new_y;
                        got_abs_event = 1;
                    }
                } else if (type == EV_KEY) {
                    if (code == BTN_LEFT) {
                        pending_btn_state = (int)val;
                        if (val == 1) pending_btn |= 1;
                        else pending_btn &= ~1;
                    } else if (code == BTN_RIGHT) {
                        if (val == 1) pending_btn |= 2;
                        else pending_btn &= ~2;
                    }
                }
            } else {
                /* Keyboard */
                if (type == EV_KEY) {
                    if (code == 29 || code == 97) {
                        if (val) pending_modifiers |= 1;
                        else pending_modifiers &= ~1;
                    } else if (code == 56 || code == 100) {
                        if (val) pending_modifiers |= 2;
                        else pending_modifiers &= ~2;
                    } else if (val == 1) {
                        if (code == 103 || code == 108 || code == 105 || code == 106) {
                            pending_arrow = code;
                        }
                        else if (code < 128) {
                            unsigned char ch = keymap[code];
                            if (ch) pending_key = ch;
                        }
                    }
                }
            }

            /* Return buffer to device */
            dev->vq.avail.ring[dev->vq.avail.idx % VIRT_QUEUE_SIZE] = (unsigned short)desc_idx;
            asm volatile("dmb sy" ::: "memory");
            dev->vq.avail.idx++;
            asm volatile("dmb sy" ::: "memory");

            dev->last_used_idx++;
        }

        /* Notify device */
        mmio_write(mmio, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    }
}

uint8_t virtio_input_getchar(void)
{
    poll_events();
    uint8_t ch = pending_key;
    if (ch) pending_key = 0;
    return ch;
}

int virtio_input_mouse(int *dx, int *dy, int *btn)
{
    poll_events();
    if (pending_dx || pending_dy || pending_btn || got_abs_event) {
        *dx = pending_dx;
        *dy = pending_dy;
        *btn = pending_btn;
        pending_dx = 0;
        pending_dy = 0;
        got_abs_event = 0;
        /* Don't clear pending_btn here — it's a state, not an edge.
         * It gets set/cleared by the event handler. */
        return 1;
    }
    return 0;
}

int virtio_input_get_modifiers(void)
{
    poll_events();
    return pending_modifiers;
}

int virtio_input_get_arrow(void)
{
    poll_events();
    int arrow = pending_arrow;
    if (arrow) pending_arrow = 0;
    return arrow;
}
