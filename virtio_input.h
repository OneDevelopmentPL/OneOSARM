/* OneOS-ARM Virtio-Input Driver (keyboard + mouse) */

#ifndef VIRTIO_INPUT_H
#define VIRTIO_INPUT_H

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

/* Virtio-MMIO register offsets */
#define VIRTIO_MMIO_MAGIC_VALUE    0x000
#define VIRTIO_MMIO_VERSION        0x004
#define VIRTIO_MMIO_DEVICE_ID      0x008
#define VIRTIO_MMIO_VENDOR_ID      0x00C
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL      0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX  0x034
#define VIRTIO_MMIO_QUEUE_NUM      0x038
#define VIRTIO_MMIO_QUEUE_ALIGN    0x03C
#define VIRTIO_MMIO_QUEUE_PFN      0x040
#define VIRTIO_MMIO_QUEUE_READY    0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY   0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK  0x064
#define VIRTIO_MMIO_STATUS         0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW  0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4

/* Virtio device IDs */
#define VIRTIO_ID_INPUT 18

/* Virtio status flags */
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8

/* Virtio-input event types */
#define EV_SYN   0x00
#define EV_KEY   0x01
#define EV_REL   0x02
#define EV_ABS   0x03

/* REL codes */
#define REL_X    0x00
#define REL_Y    0x01

/* KEY codes for special keys */
#define VKEY_UP      103
#define VKEY_DOWN    108
#define VKEY_LEFT    105
#define VKEY_RIGHT   106
#define VKEY_LEFTCTRL  29
#define VKEY_RIGHTCTRL 97
#define VKEY_LEFTALT   56
#define VKEY_RIGHTALT  100

/* Initialize virtio input devices (keyboard + mouse). Call once at boot. */
void virtio_input_init(void);

/* Poll for a keyboard ASCII character. Returns 0 if none ready. */
uint8_t virtio_input_getchar(void);

/* Poll for mouse motion. dx/dy updated if motion available. Returns 1 if got event. */
int virtio_input_mouse(int *dx, int *dy, int *btn);

/* Get modifier key states (CTRL, ALT). Returns bitmask. */
int virtio_input_get_modifiers(void);
#define MOD_CTRL 1
#define MOD_ALT  2

/* Get arrow key press. Returns 0 if none, or VKEY_UP/DOWN/LEFT/RIGHT. */
int virtio_input_get_arrow(void);

#endif
