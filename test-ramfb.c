#include <stdint.h>
struct fw_cfg_ramfb {
    uint32_t fourcc;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};
