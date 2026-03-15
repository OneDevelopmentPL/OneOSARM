/* OneOS-ARM Virtio GPU Driver */

#ifndef GPU_H
#define GPU_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

/* Initialize Virtio GPU and get framebuffer */
void gpu_init(void);

/* Get framebuffer address */
unsigned char* gpu_get_framebuffer(void);

/* Get dimensions */
void gpu_get_dimensions(int *width, int *height);

/* Flush framebuffer (for some devices) */
void gpu_flush(void);

#endif
