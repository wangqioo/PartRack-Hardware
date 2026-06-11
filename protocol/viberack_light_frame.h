#ifndef VIBERACK_LIGHT_FRAME_H
#define VIBERACK_LIGHT_FRAME_H

#include <stdint.h>

#include "viberack_protocol.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} vbrk_rgb_t;

typedef struct {
    vbrk_rgb_t slots[VBRK_SLOT_COUNT];
} vbrk_light_frame_t;

int vbrk_light_frame_build(const vbrk_light_command_t *command,
                           vbrk_light_frame_t *frame);

#endif
