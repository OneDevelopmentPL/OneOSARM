/* OneOS-ARM Virtio-Entropy (RNG) Driver */

#ifndef VIRTIO_RNG_H
#define VIRTIO_RNG_H

#include "virtio_input.h" /* Reuse some virtio-mmio constants */

/* Virtio device ID for Entropy */
#define VIRTIO_ID_RNG 4

/* Initialize virtio-rng devices. */
void virtio_rng_init(void);

/* Get random bytes from the entropy device. Returns number of bytes read. */
int virtio_rng_get_bytes(void *buf, int len);

/* Helper to get a random uint32 */
unsigned int virtio_rng_rand(void);

#endif
