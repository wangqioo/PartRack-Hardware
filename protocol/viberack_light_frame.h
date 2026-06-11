#ifndef VIBERACK_LIGHT_FRAME_H
#define VIBERACK_LIGHT_FRAME_H

#include <stdint.h>
#include <stddef.h>

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
int vbrk_light_frame_copy_pixels(const vbrk_light_frame_t *frame,
                                 vbrk_rgb_t *pixels,
                                 size_t pixel_count,
                                 uint8_t *active_slots);

#endif
